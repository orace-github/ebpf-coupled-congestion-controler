/*
 * Copyright 2019 Orace KPAKPO </ orace.kpakpo@yahoo.fr >
 *
 * This file is part of EOS.
 *
 * EOS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * EOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __LIST_H__
#define __LIST_H__

#define IS_EMPTY(x) (x)->next == NULL
#define IS_HEAD(x) (x)->prev == (x)

struct list_head
{
    struct list_head* prev;
    struct list_head* next;
};

#define LIST_HEAD_INIT(name) \
{&(name),&(name)} /* Macro defined to init list head */

#define LIST_HEAD(name) \
struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head* head)
{
    head->next = NULL;
    head->prev = head;
}

static inline void list_add(struct list_head* new, struct list_head* head)
{
    head->next = new;
    new->prev = head;
    new->next = NULL;
}

static inline void list_del(struct list_head* p)
{
    if(p->prev)
        p->prev->next = p->next;
    if(p->next) 
        p->next->prev = p->prev;

    p->next = NULL;
    p->prev = NULL;
}

static inline int list_empty(struct list_head* head)
{
    return IS_EMPTY(head);
}

#define list_entry(ptr,type,field) \
    (type*)((char*)ptr - (char*)&((type*)0)->field)

#define list_first_entry(head,type,field) \
    list_entry((head)->next,type,field)

#define list_for_each(p,head) \
    for(p = (head)->next; p != NULL; p = p->next)


#define list_for_each_safe(p,n,head) \
    for(p = (head)->next, n = p->next; p != NULL; p = n, n = n->next)

struct list
{
    struct list_head head;
    struct list_head* last_head;
};

static inline void init_list(struct list* list)
{
    INIT_LIST_HEAD(&list->head);
    list->last_head = &list->head;
}

static inline void list_remove(struct list* list, struct list_head* head)
{
    if(list->last_head == head)
        list->last_head = head->prev;

    list_del(head);
}

static inline void list_insert(struct list* list, struct list_head* head)
{
    list_add(head,list->last_head);
    if(head)
        list->last_head = head;
}

static inline void list_insert_at(struct list* list, struct list_head* new,
    struct list_head* old)
{
    if(list->last_head == old)
        list_insert(list,new);
    else
    {
        new->prev = old;
        new->next = old->next;
        old->next->prev = new;
        old->next = new;
    }
}

#endif //__LIST_H__