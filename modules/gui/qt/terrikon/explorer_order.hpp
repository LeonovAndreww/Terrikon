/*****************************************************************************
 * explorer_order.hpp: Terrikon, the order an explorer window shows a folder in
 *****************************************************************************
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef TERRIKON_EXPLORER_ORDER_HPP
#define TERRIKON_EXPLORER_ORDER_HPP

#include <QStringList>

namespace terrikon {

/* The paths of what a folder holds, in the order an open explorer window
 * shows them (its sort and its groups, as the user chose), empty if no
 * window shows that folder. Asks the explorer: call it aside, it may wait. */
QStringList explorerOrder(const QString &folder);

}

#endif
