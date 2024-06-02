//  Partie implantation du module holdall.

#include "holdall.h"

#define HOLDALL_WANT_EXT 1

//  struct holdall, holdall : implantation par liste dynamique simplement
//    chainée.

//  Si la macroconstante HOLDALL_PUT_TAIL est définie et que sa macro-évaluation
//    donne un entier non nul, l'insertion dans la liste a lieu en queue. Dans
//    le cas contraire, elle a lieu en tête.

typedef struct choldall choldall;

struct choldall {
  void *ref;
  choldall *next;
};

struct holdall {
  choldall *head;
#if defined HOLDALL_PUT_TAIL && HOLDALL_PUT_TAIL != 0
  choldall **tailptr;
#endif
  size_t count;
};

//  quicksort : trie les cellules entre head et tail (exclue) selon la fonction
//    de comparaison compar.
static void quicksort(choldall *head, choldall *tail,
    int (*compar)(const void *, const void *));

//  partition : choisit un pivot et réarrange le contenu des cellules entre
//    head et tail (exclue) avec, dans l'ordre, les éléments strictement
//    inférieur au pivot, le pivot, les éléments strictement supérieurs. Si
//    tail == NULL, toute la liste est traitée. Si la liste est vide, renvoie
//    NULL; renvoie un pointeur vers le pivot sinon.
static choldall *partition(choldall *head, choldall *tail,
    int (*compar)(const void *, const void *));

//  swap : échange le contenu des cellules pointées par p1 et p2.
static void swap(choldall *p1, choldall *p2);

holdall *holdall_empty(void) {
  holdall *ha = malloc(sizeof *ha);
  if (ha == NULL) {
    return NULL;
  }
  ha->head = NULL;
#if defined HOLDALL_PUT_TAIL && HOLDALL_PUT_TAIL != 0
  ha->tailptr = &ha->head;
#endif
  ha->count = 0;
  return ha;
}

void holdall_dispose(holdall **haptr) {
  if (*haptr == NULL) {
    return;
  }
  choldall *p = (*haptr)->head;
  while (p != NULL) {
    choldall *t = p;
    p = p->next;
    free(t);
  }
  free(*haptr);
  *haptr = NULL;
}

int holdall_put(holdall *ha, void *ref) {
  choldall *p = malloc(sizeof *p);
  if (p == NULL) {
    return -1;
  }
  p->ref = ref;
#if defined HOLDALL_PUT_TAIL && HOLDALL_PUT_TAIL != 0
  p->next = NULL;
  *ha->tailptr = p;
  ha->tailptr = &p->next;
#else
  p->next = ha->head;
  ha->head = p;
#endif
  ha->count += 1;
  return 0;
}

size_t holdall_count(holdall *ha) {
  return ha->count;
}

int holdall_apply(holdall *ha,
    int (*fun)(void *)) {
  for (const choldall *p = ha->head; p != NULL; p = p->next) {
    int r = fun(p->ref);
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

int holdall_apply_context(holdall *ha,
    void *context, void *(*fun1)(void *context, void *ptr),
    int (*fun2)(void *ptr, void *resultfun1)) {
  for (const choldall *p = ha->head; p != NULL; p = p->next) {
    int r = fun2(p->ref, fun1(context, p->ref));
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

int holdall_apply_context2(holdall *ha,
    void *context1, void *(*fun1)(void *context1, void *ptr),
    void *context2, int (*fun2)(void *context2, void *ptr, void *resultfun1)) {
  for (const choldall *p = ha->head; p != NULL; p = p->next) {
    int r = fun2(context2, p->ref, fun1(context1, p->ref));
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

#if defined HOLDALL_WANT_EXT && HOLDALL_WANT_EXT != 0

void holdall_sort(holdall *ha, int (*compar)(const void *, const void *)) {
  quicksort(ha->head, NULL, compar);
}

void quicksort(choldall *head, choldall *tail,
    int (*compar)(const void *, const void *)) {
  if (head == tail) {
    return;
  }
  choldall *p = partition(head, tail, compar);
  if (p != NULL) {
    if (head != p) {
      quicksort(head, p, compar);
    }
    if (p->next != NULL) {
      quicksort(p->next, tail, compar);
    }
  }
}

choldall *partition(choldall *head, choldall *tail,
    int (*compar)(const void *, const void *)) {
  if (head == NULL) {
    return NULL;
  }
  choldall *pivot = head;
  choldall *rep = head;
  choldall *curr = head->next;
  while (curr != tail && curr != NULL) {
    if (compar(curr->ref, pivot->ref) < 0) {
      swap(rep->next, curr);
      rep = rep->next;
    }
    curr = curr->next;
  }
  swap(pivot, rep);
  return rep;
}

void swap(choldall *p1, choldall *p2) {
  void *t = p1->ref;
  p1->ref = p2->ref;
  p2->ref = t;
}

#endif
