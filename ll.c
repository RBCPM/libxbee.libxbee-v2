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

#include <stdio.h>
#include <stdlib.h>

#include "ll.h"
#include "xsys.h"

int ll_init(struct ll_head *list) {
  list->is_head = 1;
  list->head = NULL;
  list->tail = NULL;
  list->self = list;
  if (xsys_mutex_init(&list->mutex)) return 1;
	return 0;
}

void ll_destroy(struct ll_head *list, void (*freeCallback)(void *)) {
	void *p;
	while ((p = ll_ext_tail(list)) != NULL) {
		if (freeCallback) freeCallback(p);
	}
	xsys_mutex_destroy(&list->mutex);
}

int ll_add_head(void *list, void *item) {
  struct ll_head *h;
  struct ll_info *i, *p;
  int ret;
  ret = 0;
  i = list;
  h = i->head;
  if (!(h->is_head && h->self == h)) {
    ret = 1;
    goto out;
  }
  xsys_mutex_lock(&h->mutex);
  p = h->head;
  
  if (!(h->head = calloc(1, sizeof(struct ll_info)))) {
    h->head = p;
    ret = 2;
    goto out;
  }
  h->head->head = h;
  h->head->prev = NULL;
  if (p) {
    h->head->next = p;
    p->prev = h->head;
  } else {
    h->head->next = NULL;
    h->tail = h->head;
  }
  
  h->head->item = item;
out:
  xsys_mutex_unlock(&h->mutex);
  return ret;
}
int ll_add_tail(void *list, void *item) {
  struct ll_head *h;
  struct ll_info *i, *p;
  int ret;
  ret = 0;
  i = list;
  h = i->head;
  if (!(h->is_head && h->self == h)) {
    ret = 1;
    goto out;
  }
  xsys_mutex_lock(&h->mutex);
  p = h->tail;
  
  if (!(h->tail = calloc(1, sizeof(struct ll_info)))) {
    h->tail = p;
    ret = 2;
    goto out;
  }
  h->tail->head = h;
  h->tail->next = NULL;
  if (p) {
    h->tail->prev = p;
    p->next = h->tail;
  } else {
    h->tail->prev = NULL;
    h->head = h->tail;
  }
  
  h->tail->item = item;
out:
  xsys_mutex_unlock(&h->mutex);
  return ret;
}
int ll_add_after(void *list, void *ref, void *item) {
#warning - TODO ll_add_after
  return 1;
}
int ll_add_before(void *list, void *ref, void *item) {
#warning - TODO ll_add_before
  return 1;
}

void *ll_get_head(void *list) {
  struct ll_head *h;
  struct ll_info *i;
  void *ret;
  ret = NULL;
  i = list;
  h = i->head;
  if (h->is_head && h->self == h) {
		ret = h->head->item;
	}
  return ret;
}
void *ll_get_tail(void *list) {
  struct ll_head *h;
  struct ll_info *i;
  void *ret;
  ret = NULL;
  i = list;
  h = i->head;
  if (h->is_head && h->self == h) {
		ret = h->tail->item;
	}
  return ret;
}
/* returns struct ll_info* or NULL - don't touch the pointer ;) */
void *ll_get_item(void *list, void *item) {
  struct ll_head *h;
  struct ll_info *i;
  void *ret;
  ret = NULL;
  i = list;
  h = i->head;
  if (!(h->is_head && h->self == h)) goto out;
  xsys_mutex_lock(&h->mutex);
  i = h->head;
	while (i) {
		if (i->item == item) break;
		i = i->next;
	}
	if (!i) goto out;
	ret = i;
out:
  xsys_mutex_unlock(&h->mutex);
  return ret;
}
void *ll_get_next(void *list, void *ref) {
	struct ll_info *i;
	void *ret;
	if (!(i = ll_get_item(list, ref))) goto out;
	i = i->next;
	if (!i) goto out;
	ret = i->item;
out:
	return ret;
}
void *ll_get_prev(void *list, void *ref) {
	struct ll_info *i;
	void *ret;
	if (!(i = ll_get_item(list, ref))) goto out;
	i = i->prev;
	if (!i) goto out;
	ret = i->item;
out:
	return ret;
}

void *ll_ext_head(void *list) {
  struct ll_head *h;
  struct ll_info *i, *p;
  void *ret;
  ret = NULL;
  i = list;
  h = i->head;
  if (!(h->is_head && h->self == h)) goto out;
  xsys_mutex_lock(&h->mutex);
  p = h->head;
  if (p) {
    ret = p->item;
    
    h->head = p->next;
    if (h->head) h->head->prev = NULL;
    if (h->tail == p) h->tail = NULL;
    free(p);
  }
out:
  xsys_mutex_unlock(&h->mutex);
  return ret;
}
void *ll_ext_tail(void *list) {
  struct ll_head *h;
  struct ll_info *i, *p;
  void *ret;
  ret = NULL;
  i = list;
  h = i->head;
  if (!(h->is_head && h->self == h)) goto out;
  xsys_mutex_lock(&h->mutex);
  p = h->tail;
  if (p) {
    ret = p->item;
    
    h->tail = p->next;
    if (h->tail) h->tail->next = NULL;
    if (h->head == p) h->head = NULL;
    free(p);
  }
out:
  xsys_mutex_unlock(&h->mutex);
  return ret;
}
int ll_ext_item(void *list, void *item) {
  struct ll_head *h;
  struct ll_info *i, *p;
  int ret;
  ret = 1;
  i = list;
  h = i->head;
  if (!item) return 0;
  if (!(h->is_head && h->self == h)) goto out;
  xsys_mutex_lock(&h->mutex);
  p = h->head;
  while (p) {
    if (p->is_head) {
      ret = 2;
      break;
    }
    if (p->item == item) {
      
      if (p->next) {
        p->next->prev = p->prev;
      } else {
        h->tail = p->prev;
      }
      if (p->prev) {
        p->prev->next = p->next;
      } else {
        h->head = p->next;
      }
      
      free(p);
      
      ret = 0;
      goto out;
      
    }
    p = p->next;
  }
out:
  xsys_mutex_unlock(&h->mutex);
  return ret;
}

int ll_count_items(void *list) {
  struct ll_head *h;
  struct ll_info *i, *p;
  int ret;
  ret = -1;
  i = list;
  h = i->head;
  if (!(h->is_head && h->self == h)) goto out;
  xsys_mutex_lock(&h->mutex);
  p = h->head;
  ret = 0;
  while (p) {
    ret++;
    p = p->next;
  }
out:
  xsys_mutex_unlock(&h->mutex);
  return ret;
}
