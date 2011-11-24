#ifndef __XBEE_ERRORS_H
#define __XBEE_ERRORS_H

/*
  libxbee - a C library to aid the use of Digi's Series 1 XBee modules
            running in API mode (AP=2).

  Copyright (C) 2009  Attie Grande (attie@attie.co.uk)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define XBEE_ENONE                                           0
#define XBEE_EUNKNOWN                                       -1
#define XBEE_ENOMEM                                         -2
#define XBEE_ESELECT                                        -3
#define XBEE_ESELECTINTERRUPTED                             -4
#define XBEE_EEOF                                           -5
#define XBEE_EIORETRIES                                     -6
#define XBEE_EOPENFAILED                                    -7
#define XBEE_EIO                                            -8
#define XBEE_ESEMAPHORE                                     -9
#define XBEE_ELINKEDLIST                                   -10
#define XBEE_EPTHREAD                                      -11
#define XBEE_ENOXBEE                                       -12
#define XBEE_EMISSINGPARAM                                 -13
#define XBEE_EINVALBAUDRATE                                -14
#define XBEE_ESETUP                                        -15
#define XBEE_ELENGTH                                       -16
#define XBEE_EINVAL                                        -17

#endif /* __XBEE_ERRORS_H */

