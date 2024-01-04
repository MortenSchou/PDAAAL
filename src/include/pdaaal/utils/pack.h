/* 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  Copyright Morten K. Schou
 */

/* 
 * File:   pack.h
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 04-01-2023.
 */

#ifndef PDAAAL_PACK_H
#define PDAAAL_PACK_H

// Provides a PACK(...) macro to used packed structs in a platform independent way.

#ifdef __GNUC__
#define PACK(...) __VA_ARGS__ __attribute__((__packed__))
#elif defined(_MSC_VER)
#define PACK(...) __pragma( pack(push, 1) ) __VA_ARGS__ __pragma( pack(pop))
#else
#define PACK(...) __VA_ARGS__
#endif

#endif //PDAAAL_PACK_H
