#ifndef __XBEE_LL_H
#define __XBEE_LL_H

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

#include <pthread.h>
#include <semaphore.h>

struct ll_head {
  struct ll_info *head;
  struct ll_info *tail;
  int is_head;
  struct ll_head *self;
  pthread_mutex_t mutex;
	sem_t sem;
};

struct ll_info {
  struct ll_info *next;
  struct ll_info *prev;
  int is_head;
  struct ll_head *head;
  void *item;
};

void ll_init(struct ll_head *list);

int ll_add_head(void *list, void *item);
int ll_add_tail(void *list, void *item);
int ll_add_after(void *list, void *ref, void *item); /* dead */
int ll_add_before(void *list, void *ref, void *item); /* dead */

void *ll_get_head(void *list);
void *ll_get_tail(void *list);
void *ll_get_item(void *list, void *item); /* dead */
void *ll_get_next(void *list, void *ref); /* dead */
void *ll_get_prev(void *list, void *ref); /* dead */

void *ll_ext_head(void *list);
void *ll_ext_tail(void *list);
int ll_ext_item(void *list, void *item);

int ll_count_items(void *list);

#endif /* __XBEE_LL_H */
