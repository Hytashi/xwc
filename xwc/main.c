#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <locale.h>

#include "hashtable.h"
#include "holdall.h"
#include "sbuffer.h"

#define STR(s)  #s
#define XSTR(s) STR(s)

#define STDIN_FNAME "-"
#define FORMAT_FILE_NAME(f) (strcmp((f), STDIN_FNAME) == 0 ? "\"\"" : f)

#define PRINT_READ_ERR(f)                                                      \
  if (strcmp(f, STDIN_FNAME) == 0) {                                           \
    fprintf(stderr, "Error: An error has occurred while reading on stdin\n");  \
  } else {                                                                     \
    fprintf(stderr, "Error: An error has occurred while reading file '%s'\n",  \
    f);                                                                        \
  }

#define CRESET        "\x1b[0m"
#define CHIGHLIGHT    "\x1b[107;30m"

#define OPT_PARSE_ERR(msg, opt)                                                \
  fprintf(stderr, "%s: %s -- '%c'\n", argv[0], msg, opt);                      \
  suggest_help(argv[0]);                                                       \
  exit(EXIT_FAILURE);

#define RESTRICT_FILE_INDEX       0
#define INPUT_FILE_START_INDEX    1

#define MAX_LINE_LEN      80
#define HELP_DOC_COLUMN   16

#define PRINT_WHITESPACE(count)                                                \
  if (count != 0) {                                                            \
    printf("%*c", (int) count, ' ');                                           \
  }

#define OPT_GROUP_CHAR  '\0'
#define OPT_END ((opt) {OPT_GROUP_CHAR, NULL, NULL, false})
#define DEF_OPT(c, doc, group_prev) ((opt) {c, NULL, doc, group_prev})
#define DEF_OPT_ARG(c, arg, doc, group_prev) ((opt) {c, arg, doc, group_prev})
#define DEF_GROUP(doc) ((opt) {OPT_GROUP_CHAR, NULL, doc, false})

#define OPT_INITIAL       'i'
#define OPT_PUNCT         'p'
#define OPT_RESTRICT      'r'
#define OPT_SORT          's'
#define OPT_SORT_STR      "s"
#define OPT_SORT_LEX      'l'
#define OPT_SORT_NONE     'S'
#define OPT_REVERSE       'R'
#define OPT_HELP          '?'

#define OPT_ARG_SORT_LEX  "lexicographical"
#define OPT_ARG_SORT_NONE "none"

//- STRUCTURES -----------------------------------------------------------------

//  options : type et nom de type pour une structure contenant les valeurs
//    des options rentrées par l'utilisateurice.
typedef struct {
  char *restr_f;
  bool punct;
  size_t init;
  enum {
    NONE,
    LEXICOGRAPHICAL
  } sort_mode;
  bool sort_reversed;
} options;

//  word_info : type et nom de type pour une structure contenant les
//    informations sur un mot lu.
typedef struct {
  long int occ;
  size_t file;
} word_info;

//  opt : type et nom de type pour une structure représentant une option
//    utilisable sur la ligne de commande.
typedef struct {
  char c;
  const char *arg;
  const char *doc;
  bool group_prev;
} opt;

//- PROTOTYPES -----------------------------------------------------------------

//  str_hashfun : l'une des fonctions de pré-hachage conseillées par Kernighan
//    et Pike pour les chaines de caractères.
static size_t str_hashfun(const char *s);

//  rprint_word_info : affiche sur la sortie standard la chaîne de caractères
//    pointée par w dans la première colonne, puis le nombre d'occurrences dans
//    la colonne correspondant au fichier dans lequel le mot apparaît, enfin
//    retourne 0.
static int rprint_word_info(char *w, word_info *wi);

//  rfree_word_info : libère la zone mémoire pointée par wi et retourne 0.
static int rfree_word_info(char *w, word_info *wi);

//  rfree : libère la zone mémoire pointée par w et retourne 0.
static int rfree(char *w);

//  rev_strcoll : renvoie l'inverse de strcoll(s1, s2).
static int rev_strcoll(const char *s1, const char *s2);

//  print_usage : affiche sur la sortie standard un court message expliquant
//    l'utilisation du programme dont le nom de l'exécutable est prog_name.
static void print_usage(char *prog_name);

//  print_usage : affiche sur la sortie standard un court message suggérant
//    d'utiliser la commande d'aide du programme dont le nom de l'exécutable est
//    prog_name.
static void suggest_help(char *prog_name);

//  print_help : affiche sur la sortie standard un message détaillant le but du
//    programme dont le nom de l'exécutable est prog_name, son utilisation et
//    l'ensemble des options spécifiées dans opts.
static void print_help(char *prog_name, opt opts[]);

//  print_multi_line : affiche sur la sortie standard la chaîne de caractères s
//    en la découpant de sorte à ce que chaque ligne ne dépasse pas MAX_LINE_LEN
//    caractères. Chaque ligne, sauf la première, est précédée par pref_whitesp
//    espaces, qui comptent dans le calcul de la longueur de la ligne. Pour une
//    utilisation interne, le fonctionnement est limité : seul l'espace est
//    reconnu comme séparateur; on suppose pref_whitesp < MAX_LINE_LEN; on
//    suppose qu'aucun mot ne fait plus de MAX_LINE_LEN - pref_whitesp
//    caractères.
static void print_multi_line(size_t pref_whitesp, const char *s);

//- MAIN -----------------------------------------------------------------------

int main(int argc, char **argv) {
  int r = EXIT_SUCCESS;
  setlocale(LC_COLLATE, "");
  opt opts[] = {
    DEF_GROUP("Program Information:"),
    DEF_OPT(OPT_HELP, "Print this help message and exit.", true),
    DEF_GROUP("Input Control:"),
    DEF_OPT_ARG(OPT_INITIAL, "VALUE", "Set the maximal number of "
        "significant initial letters for words to VALUE. 0 means without "
        "limitation. Default is 0.", true),
    DEF_OPT(OPT_PUNCT, "Make the punctuation characters play the"
        "same role as white-space characters in the meaning of words.", false),
    DEF_OPT_ARG(OPT_RESTRICT, "FILE", "Limit the counting to the set of words "
        "that appear in FILE. FILE is displayed in the first column of the "
        "header line. If FILE is \"-\", read words from the standard input; in "
        "this case, \"\" is displayed in first column of the header line.",
        false),
    DEF_GROUP("Output Control:"),
    DEF_OPT_ARG(OPT_SORT, "TYPE", "Sort the results in ascending order, by "
        "default, according to TYPE. The available values for TYPE are: '"
        OPT_ARG_SORT_LEX "', sort on words, and '" OPT_ARG_SORT_NONE "', don't "
        "try to sort, take it as it comes. Default is '" OPT_ARG_SORT_NONE "'.",
        true),
    DEF_OPT(OPT_SORT_LEX, "Same as -" OPT_SORT_STR " " OPT_ARG_SORT_LEX ".",
        false),
    DEF_OPT(OPT_SORT_NONE, "Same as -" OPT_SORT_STR " " OPT_ARG_SORT_NONE ".",
        true),
    DEF_OPT(OPT_REVERSE, "Sort in descending order on the single or first "
        "key instead of ascending order. This option has no effect if sorting "
        "is disabled.",
        false),
    OPT_END
  };
  char optstr[2 * (sizeof opts / sizeof *opts - 1)];
  size_t str_i = 0;
  for (size_t k = 0; opts[k].c != OPT_GROUP_CHAR || opts[k].doc != NULL; k++) {
    if (opts[k].c == '?' || opts[k].c == OPT_GROUP_CHAR) {
      continue;
    }
    optstr[str_i] = opts[k].c;
    str_i++;
    if (opts[k].arg != NULL) {
      optstr[str_i] = ':';
      str_i++;
    }
  }
  optstr[str_i] = '\0';
  options p = {
    .restr_f = NULL,
    .punct = false,
    .init = 0,
    .sort_mode = NONE,
    .sort_reversed = false
  };
  opterr = 0;
  int c;
  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
      case OPT_PUNCT:
        p.punct = true;
        break;
      case OPT_RESTRICT:
        p.restr_f = optarg;
        break;
      case OPT_REVERSE:
        p.sort_reversed = true;
        break;
      case OPT_SORT_LEX:
        p.sort_mode = LEXICOGRAPHICAL;
        break;
      case OPT_SORT_NONE:
        p.sort_mode = NONE;
        break;
      case OPT_SORT:
        if (strcmp(OPT_ARG_SORT_LEX, optarg) == 0) {
          p.sort_mode = LEXICOGRAPHICAL;
        } else if (strcmp(OPT_ARG_SORT_NONE, optarg) == 0) {
          p.sort_mode = NONE;
        } else {
          OPT_PARSE_ERR("option value not recognized", c);
        }
        break;
      case OPT_INITIAL:
        char *end;
        long int v = strtol(optarg, &end, 10);
        if (*end != '\0') {
          OPT_PARSE_ERR("option requires an integer argument", c);
        } else if (errno == ERANGE
            || (v >= 0 && (unsigned long int) v > SIZE_MAX)) {
          OPT_PARSE_ERR("option argument is too large", c);
        } else if (v < 0) {
          OPT_PARSE_ERR("option requires a positive integer argument", c);
        } else {
          p.init = (size_t) v;
        }
        break;
      case '?':
        if (optopt == '?') {
          print_help(argv[0], opts);
          return EXIT_SUCCESS;
        } else {
          char *p = strchr(optstr, optopt);
          if (p != NULL && *(p + 1) == ':') {
            OPT_PARSE_ERR("option requires an argument", optopt);
          } else {
            OPT_PARSE_ERR("invalid option", optopt);
          }
        }
    }
  }
  hashtable *ht = hashtable_empty((int (*)(const void *, const void *))strcmp,
      (size_t (*)(const void *))str_hashfun);
  holdall *has = holdall_empty();
  sbuffer *sb = sbuffer_empty();
  if (ht == NULL || has == NULL || sb == NULL) {
    goto error_capacity;
  }
  int i = optind;
  size_t nfile = INPUT_FILE_START_INDEX;
  if (p.restr_f != NULL) {
    i -= 1;
    nfile = RESTRICT_FILE_INDEX;
  }
  for (; i < (optind == argc ? argc + 1 : argc); i++) {
    const char *fname;
    if (nfile == RESTRICT_FILE_INDEX) {
      fname = p.restr_f;
    } else if (i >= argc) {
      fname = STDIN_FNAME;
    } else {
      fname = argv[i];
    }
    FILE *f;
    if (strcmp(fname, STDIN_FNAME) == 0) {
      f = stdin;
      printf(CHIGHLIGHT "--- starts reading for ");
      if (nfile == RESTRICT_FILE_INDEX) {
        printf("restrict");
      } else {
        printf("#%zu", nfile);
      }
      printf(" FILE" CRESET "\n");
    } else {
      f = fopen(fname, "r");
    }
    if (f == NULL) {
      PRINT_READ_ERR(fname);
      goto error;
    }
    sbuffer_clear(sb);
    bool run = true;
    bool skip_chr = false;
    int c = fgetc(f);
    while (run) {
      bool cutlen = p.init != 0 && sbuffer_length(sb) == p.init;
      if (isspace(c) || (p.punct && ispunct(c)) || c == EOF || cutlen) {
        if (c == EOF) {
          run = false;
        }
        if (skip_chr) {
          skip_chr = false;
          goto next_char;
        }
        if (sbuffer_length(sb) == 0) {
          goto next_char;
        }
        if (sbuffer_append(sb, '\0') != 0) {
          goto error_capacity;
        }
        char *w = sbuffer_get_str(sb);
        if (cutlen) {
          skip_chr = true;
          fprintf(stderr, "%s: Word from ", argv[0]);
          if (f == stdin) {
            fprintf(stderr, "standard input");
          } else {
            fprintf(stderr, "file '%s'", fname);
          }
          fprintf(stderr, " cut: '%s...'.\n", w);
        }
        word_info *wi = hashtable_search(ht, w);
        if (wi == NULL) {
          if (nfile != RESTRICT_FILE_INDEX && p.restr_f != NULL) {
            sbuffer_clear(sb);
            goto next_char;
          }
          char *w2 = malloc(sbuffer_length(sb) * sizeof *w2);
          if (w2 == NULL) {
            goto error_capacity;
          }
          strcpy(w2, w);
          if (holdall_put(has, w2) != 0) {
            goto error_capacity;
          }
          wi = malloc(sizeof *wi);
          if (wi == NULL) {
            goto error_capacity;
          }
          if (hashtable_add(ht, w2, wi) == NULL) {
            free(wi);
            goto error_capacity;
          }
          wi->file = nfile;
          wi->occ = (nfile == RESTRICT_FILE_INDEX ? 0 : 1);
        } else {
          if (nfile == RESTRICT_FILE_INDEX) {
            sbuffer_clear(sb);
            goto next_char;
          }
          if (wi->file != nfile) {
            if (wi->file == RESTRICT_FILE_INDEX) {
              wi->file = nfile;
              wi->occ = 1;
            } else {
              wi->occ = 0;
            }
          } else {
            wi->occ += 1;
          }
        }
        sbuffer_clear(sb);
      } else {
        if (skip_chr) {
          goto next_char;
        }
        if (sbuffer_append(sb, (char) c) != 0) {
          goto error_capacity;
        }
      }
next_char:
      c = fgetc(f);
    }
    if (!feof(f)) {
      if (f != stdin) {
        fclose(f);
      }
      PRINT_READ_ERR(fname);
      goto error;
    }
    if (f == stdin) {
      printf(CHIGHLIGHT "--- ends reading for ");
      if (nfile == RESTRICT_FILE_INDEX) {
        printf("restrict");
      } else {
        printf("#%zu", nfile);
      }
      printf(" FILE" CRESET "\n");
      clearerr(stdin);
    } else {
      if (fclose(f) != 0) {
        PRINT_READ_ERR(fname);
        goto error;
      }
    }
    nfile++;
  }
  if (p.sort_mode == LEXICOGRAPHICAL) {
    if (p.sort_reversed) {
      holdall_sort(has, (int (*)(const void *, const void *))rev_strcoll);
    } else {
      holdall_sort(has, (int (*)(const void *, const void *))strcoll);
    }
  }
  if (p.restr_f != NULL) {
    printf("%s", FORMAT_FILE_NAME(p.restr_f));
  }
  if (optind == argc) {
    printf("\t%s", FORMAT_FILE_NAME(STDIN_FNAME));
  } else {
    for (int i = optind; i < argc; i++) {
      printf("\t%s", FORMAT_FILE_NAME(argv[i]));
    }
  }
  printf("\n");
  holdall_apply_context(has, ht,
      (void *(*)(void *, void *))hashtable_search,
      (int (*)(void *, void *))rprint_word_info);
  goto dispose;
error_capacity:
  fprintf(stderr, "Error: Not enough memory\n");
  goto error;
error:
  r = EXIT_FAILURE;
  goto dispose;
dispose:
  if (has != NULL) {
    if (ht != NULL) {
      holdall_apply_context(has, ht,
          (void *(*)(void *, void *))hashtable_search,
          (int (*)(void *, void *))rfree_word_info);
      hashtable_dispose(&ht);
    }
    holdall_apply(has, (int (*)(void *))rfree);
    holdall_dispose(&has);
  }
  sbuffer_dispose(&sb);
  return r;
}

//- UTILITAIRES ----------------------------------------------------------------

int rprint_word_info(char *w, word_info *wi) {
  if (wi->occ != 0) {
    printf("%s", w);
    for (size_t i = 0; i < wi->file; i++) {
      printf("\t");
    }
    printf("%ld\n", wi->occ);
  }
  return 0;
}

int rfree_word_info([[maybe_unused]] char *w, word_info *wi) {
  free(wi);
  return 0;
}

int rfree(char *w) {
  free(w);
  return 0;
}

int rev_strcoll(const char *s1, const char *s2) {
  return -1 * strcoll(s1, s2);
}

size_t str_hashfun(const char *s) {
  size_t h = 0;
  for (const unsigned char *p = (const unsigned char *) s; *p != '\0'; ++p) {
    h = 37 * h + *p;
  }
  return h;
}

//- AIDES ----------------------------------------------------------------------

void print_usage(char *prog_name) {
  printf("Usage: %s [OPTION]... [FILE]...\n", prog_name);
}

void suggest_help(char *prog_name) {
  printf("Try '%s -?' for more information.\n", prog_name);
}

void print_help(char *prog_name, opt opts[]) {
  print_usage(prog_name);
  print_multi_line(0, "\nExclusive word counting. Print the number of "
      "occurrences of each word that appears in one and only one of given text "
      "FILES.\n\nA word is, by default, a maximum length sequence of "
      "characters that do not belong to the white-space characters set.\n\n"
      "Results are displayed in columns on the standard output. Columns are "
      "separated by the tab character. Lines are terminated by the end-of-line "
      "character. A header line shows the FILE names: the name of the first "
      "FILE appears in the second column, that of the second in the third, and "
      "so on. For the following lines, a word appears in the first column, its "
      "number of occurrences in the FILE in which it appears to the exclusion "
      "of all others in the column associated with the FILE. No tab characters "
      "are written on a line after the number of occurrences.\n\nRead the "
      "standard input when no FILE is given or for any FILE which is \"-\". In "
      "such cases, \"\" is displayed in the column associated with the FILE on "
      "the header line.\n\nThe locale specified by the environment affects "
      "sort order. Set 'LC_ALL=C' to get the traditional sort order that uses "
      "native byte values.\n");
  size_t k = 0;
  while (opts[k].c != OPT_END.c || opts[k].doc != OPT_END.doc) {
    if (opts[k].c == OPT_GROUP_CHAR) {
      printf("\n %s\n", opts[k].doc);
    } else {
      if (!opts[k].group_prev) {
        printf("\n");
      }
      printf("  -%c ", opts[k].c);
      if (opts[k].arg != NULL) {
        printf("%s\t", opts[k].arg);
      } else {
        printf("\t\t");
      }
      print_multi_line(HELP_DOC_COLUMN, opts[k].doc);
      printf("\n");
    }
    k++;
  }
}

void print_multi_line(size_t pref_whitesp, const char *s) {
  size_t len = pref_whitesp;
  char w[MAX_LINE_LEN - pref_whitesp + 1];
  size_t k = 0;
  for (size_t i = 0; s[i] != '\0'; i++) {
    if (len == 0) {
      PRINT_WHITESPACE(pref_whitesp);
      len = pref_whitesp;
    }
    if (s[i] == '\n' || s[i] == ' ') {
      if (len + k + (s[i] == ' ' ? 1 : 0) > MAX_LINE_LEN) {
        printf("\n");
        PRINT_WHITESPACE(pref_whitesp);
        len = pref_whitesp;
      }
      w[k] = '\0';
      printf("%s%c", w, s[i]);
      len = (s[i] == ' ' ? len + k + 1 : 0);
      k = 0;
    } else {
      w[k] = s[i];
      k++;
    }
  }
  if (k != 0) {
    if (k + len > MAX_LINE_LEN) {
      printf("\n");
      PRINT_WHITESPACE(pref_whitesp);
    }
    w[k] = '\0';
    printf("%s", w);
  }
}
