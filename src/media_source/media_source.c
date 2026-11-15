/*****************************************************************************
 * media_source.c
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "media_source.h"

#include <assert.h>
#include <stdatomic.h>
#include <vlc_atomic.h>
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>
#include <vlc_vector.h>
#include "../libvlc.h"

#ifdef TEST_MEDIA_SOURCE
#define vlc_module_name "test"
#endif /* TEST_MEDIA_SOURCE */

typedef struct
{
    vlc_media_source_t public_data;

    services_discovery_t *sd;
    /* Raised from zero (found in the list) and dropped to zero with the
     * provider locked; otherwise changed without it. */
    atomic_size_t rc;
    vlc_media_source_provider_t *owner;
    struct vlc_list node;
    char name[];
} media_source_private_t;

#define ms_priv(ms) container_of(ms, media_source_private_t, public_data)

struct vlc_media_source_provider_t
{
    struct vlc_object_t obj;
    vlc_mutex_t lock;
    struct vlc_list media_sources;
};

/* A new item has been added to a certain services discovery */
static void
services_discovery_item_added(services_discovery_t *sd,
                              input_item_t *parent, input_item_t *media)
{
    vlc_media_source_t *ms = sd->owner.sys;
    vlc_media_tree_t *tree = ms->tree;

    msg_Dbg(sd, "adding: %s", media->psz_name ? media->psz_name : "(null)");

    vlc_media_tree_Lock(tree);

    input_item_node_t *parent_node;
    if (parent)
        vlc_media_tree_Find(tree, parent, &parent_node, NULL);
    else
        parent_node = &tree->root;

    bool added = vlc_media_tree_Add(tree, parent_node, media) != NULL;
    if (unlikely(!added))
        msg_Err(sd, "could not allocate media tree node");

    vlc_media_tree_Unlock(tree);
}

static void
services_discovery_item_removed(services_discovery_t *sd, input_item_t *media)
{
    vlc_media_source_t *ms = sd->owner.sys;
    vlc_media_tree_t *tree = ms->tree;

    msg_Dbg(sd, "removing: %s", media->psz_name ? media->psz_name : "(null)");

    vlc_media_tree_Lock(tree);
    bool removed = vlc_media_tree_Remove(tree, media);
    vlc_media_tree_Unlock(tree);

    if (unlikely(!removed))
    {
        msg_Err(sd, "removing item not added"); /* SD plugin bug */
        return;
    }
}

static const struct services_discovery_callbacks sd_cbs = {
    .item_added = services_discovery_item_added,
    .item_removed = services_discovery_item_removed,
};

static vlc_media_source_t *
vlc_media_source_New(vlc_media_source_provider_t *provider, const char *name)
{
    media_source_private_t *priv = malloc(sizeof(*priv) + strlen(name) + 1);
    if (unlikely(!priv))
        return NULL;

    atomic_init(&priv->rc, 1);

    vlc_media_source_t *ms = &priv->public_data;

    /* vlc_sd_Create() may call services_discovery_item_added(), which will read
     * its tree, so it must be initialized first */
    ms->tree = vlc_media_tree_New();
    if (unlikely(!ms->tree))
    {
        free(priv);
        return NULL;
    }

    strcpy(priv->name, name);

    struct services_discovery_owner_t owner = {
        .cbs = &sd_cbs,
        .sys = ms,
    };

    priv->sd = vlc_sd_Create(provider, name, &owner);
    if (unlikely(!priv->sd))
    {
        vlc_media_tree_Release(ms->tree);
        free(priv);
        return NULL;
    }

    /* sd->description is set during vlc_sd_Create() */
    ms->description = priv->sd->description;

    priv->owner = provider;

    return ms;
}


static void
vlc_media_source_Delete(vlc_media_source_t *ms)
{
    media_source_private_t *priv = ms_priv(ms);

    vlc_sd_Destroy(priv->sd);
    vlc_media_tree_Release(ms->tree);
    free(priv);
}

void
vlc_media_source_Hold(vlc_media_source_t *ms)
{
    media_source_private_t *priv = ms_priv(ms);

    /* Without the provider's lock: it is kept while a services discovery is
     * opened (vlc_media_source_provider_GetMediaSource()), for as long as
     * that takes, and this is called from interface threads. The caller
     * owns a reference: the count cannot fall to zero meanwhile. */
    size_t rc = atomic_fetch_add_explicit(&priv->rc, 1, memory_order_relaxed);
    vlc_assert(rc != 0);
    (void) rc;
}

void
vlc_media_source_Release(vlc_media_source_t *ms)
{
    media_source_private_t *priv = ms_priv(ms);
    vlc_media_source_provider_t *provider = priv->owner;

    /* Not the last reference: no need for the lock (see
     * vlc_media_source_Hold()) */
    size_t rc = atomic_load(&priv->rc);
    vlc_assert(rc != 0);
    while (rc > 1)
        if (atomic_compare_exchange_weak(&priv->rc, &rc, rc - 1))
            return;

    /* The last one, unless the source is found again meanwhile */
    vlc_mutex_lock(&provider->lock);
    if (atomic_fetch_sub(&priv->rc, 1) != 1)
    {
        vlc_mutex_unlock(&provider->lock);
        return;
    }
    vlc_list_remove(&priv->node);
    vlc_mutex_unlock(&provider->lock);

    vlc_media_source_Delete(ms);
}

static vlc_media_source_t *
vlc_media_source_provider_Find(vlc_media_source_provider_t *provider,
                               const char *name)
{
    vlc_mutex_assert(&provider->lock);
    media_source_private_t *entry;
    vlc_list_foreach(entry, &provider->media_sources, node)
        if (!strcmp(name, entry->name))
            return &entry->public_data;
    return NULL;
}

vlc_media_source_provider_t *
vlc_media_source_provider_Get(libvlc_int_t *libvlc)
{
    return libvlc_priv(libvlc)->media_source_provider;
}

static void *
CreateObject(vlc_object_t *parent, size_t length, const char *typename)
{
#ifdef TEST_MEDIA_SOURCE
    VLC_UNUSED(parent);
    VLC_UNUSED(typename);
    return malloc(length);
#else
    return vlc_custom_create(parent, length, typename);
#endif
}

static void
ReleaseObject(void *obj)
{
#ifdef TEST_MEDIA_SOURCE
    free(obj);
#else
    vlc_object_delete((vlc_media_source_provider_t *)obj);
#endif
}

#undef vlc_media_source_provider_New
vlc_media_source_provider_t *
vlc_media_source_provider_New(vlc_object_t *parent)
{
    vlc_media_source_provider_t *provider =
            CreateObject(parent, sizeof(*provider), "media-source-provider");
    if (unlikely(!provider))
        return NULL;

    vlc_mutex_init(&provider->lock);
    vlc_list_init(&provider->media_sources);
    return provider;
}

void
vlc_media_source_provider_Delete(vlc_media_source_provider_t *provider)
{
    ReleaseObject(provider);
}

static vlc_media_source_t *
vlc_media_source_provider_Add(vlc_media_source_provider_t *provider,
                              const char *name)
{
    vlc_mutex_assert(&provider->lock);

    vlc_media_source_t *ms = vlc_media_source_New(provider, name);
    if (unlikely(!ms))
        return NULL;

    vlc_list_append(&ms_priv(ms)->node, &provider->media_sources);
    return ms;
}

vlc_media_source_t *
vlc_media_source_provider_GetMediaSource(vlc_media_source_provider_t *provider,
                                         const char *name)
{
    vlc_mutex_lock(&provider->lock);
    vlc_media_source_t *ms = vlc_media_source_provider_Find(provider, name);
    if (ms)
    {
        media_source_private_t *priv = ms_priv(ms);
        size_t rc = atomic_fetch_add(&priv->rc, 1);
        vlc_assert(rc != 0);
        (void) rc;
    }
    else
        ms = vlc_media_source_provider_Add(provider, name);
    vlc_mutex_unlock(&provider->lock);

    return ms;
}

struct vlc_media_source_meta_list
{
    struct VLC_VECTOR(struct vlc_media_source_meta) vec;
};

struct vlc_media_source_meta_list *
vlc_media_source_provider_List(vlc_media_source_provider_t *provider,
                               enum services_discovery_category_e category)
{
    char **longnames;
    int *categories;
    char **names = vlc_sd_GetNames(provider, &longnames, &categories);
    if (!names)
        /* vlc_sd_GetNames() returns NULL both on error or no result */
        return NULL;

    struct vlc_media_source_meta_list *list = malloc(sizeof(*list));
    if (unlikely(!list))
        return NULL;

    vlc_vector_init(&list->vec);
    for (size_t i = 0; names[i]; ++i)
    {
        if (category && categories[i] != (int) category)
        {
            free(names[i]);
            free(longnames[i]);
            /* only list items for the requested category */
            continue;
        }

        struct vlc_media_source_meta meta = {
            .name = names[i],
            .longname = longnames[i],
            .category = categories[i],
        };
        bool ok = vlc_vector_push(&list->vec, meta);
        if (unlikely(!ok)) {
            /* failure, clean up */
            for (char **p = names; *p; ++p)
                free(*p);
            for (char **p = longnames; *p; ++p)
                free(*p);
            vlc_vector_destroy(&list->vec);
            free(list);
            list = NULL;
            break;
        }
    }

    free(names);
    free(longnames);
    free(categories);

    return list;
}

size_t
vlc_media_source_meta_list_Count(vlc_media_source_meta_list_t *list)
{
    return list->vec.size;
}

struct vlc_media_source_meta *
vlc_media_source_meta_list_Get(vlc_media_source_meta_list_t *list, size_t index)
{
    return &list->vec.data[index];
}

void
vlc_media_source_meta_list_Delete(vlc_media_source_meta_list_t *list) {
    for (size_t i = 0; i < list->vec.size; ++i)
    {
        free(list->vec.data[i].name);
        free(list->vec.data[i].longname);
    }
    vlc_vector_destroy(&list->vec);
    free(list);
}
