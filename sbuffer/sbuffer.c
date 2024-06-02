//  sbuffer.c : partie implantation d'un module pour la gestion d'une chaîne de
//    caractères de longueur extensible, ou string buffer.

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "sbuffer.h"

#define BUFF__CAPACITY_MIN  4
#define BUFF__CAPACITY_MUL  2

struct sbuffer {
  char *array;
  size_t length;
  size_t capacity;
};

sbuffer *sbuffer_empty() {
  sbuffer *sb = malloc(sizeof *sb);
  if (sb == NULL) {
    return NULL;
  }
  char *arr = malloc(BUFF__CAPACITY_MIN * sizeof *arr);
  if (arr == NULL) {
    free(sb);
    return NULL;
  }
  sb->array = arr;
  sb->length = 0;
  sb->capacity = BUFF__CAPACITY_MIN;
  return sb;
}

void sbuffer_dispose(sbuffer **sb) {
  if (*sb == NULL) {
    return;
  }
  free((*sb)->array);
  free(*sb);
  *sb = NULL;
}

size_t sbuffer_length(sbuffer *sb) {
  return sb->length;
}

void sbuffer_clear(sbuffer *sb) {
  sb->length = 0;
}

int sbuffer_append(sbuffer *sb, char c) {
  if (sb->length > 0 && sb->array[sb->length - 1] == '\0') {
    return 1;
  }
  if (sb->length == sb->capacity) {
    if (sb->capacity * sizeof *sb->array > SIZE_MAX / BUFF__CAPACITY_MUL) {
      return 2;
    }
    char *arr = realloc(sb->array, sb->capacity * BUFF__CAPACITY_MUL
        * sizeof *sb->array);
    if (arr == NULL) {
      free(arr);
      return 3;
    }
    sb->array = arr;
    sb->capacity *= BUFF__CAPACITY_MUL;
  }
  sb->array[sb->length] = c;
  sb->length += 1;
  return 0;
}

char *sbuffer_get_str(sbuffer *sb) {
  if (sb->length == 0 || sb->array[sb->length - 1] != '\0') {
    if (sbuffer_append(sb, '\0') != 0) {
      return NULL;
    }
  }
  return sb->array;
}
