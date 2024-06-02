//  sbuffer.h : partie interface d'un module pour la gestion d'une chaîne de
//    caractères de longueur extensible, ou string buffer.

#ifndef SBUFFER__H
#define SBUFFER__H

//  struct sbuffer, sbuffer : type et nom de type d'un contrôleur regroupant
//    les informations nécessaires pour gérer un buffer de caractères.
typedef struct sbuffer sbuffer;

//  sbuffer_empty : tente d'allouer les ressources nécessaires pour gérer un
//    nouveau buffer de caractères. Renvoie NULL en cas de dépassement de
//    capacité. Renvoie sinon un pointeur vers le contrôleur associé au buffer.
extern sbuffer *sbuffer_empty();

//  sbuffer_dispose : sans effet si *sb vaut NULL. Libère sinon les
//    ressources allouées à la gestion du buffer associé à *sb
//    puis affecte NULL à *sb.
extern void sbuffer_dispose(sbuffer **sb);

//  sbuffer_length : renvoie la longueur de la chaîne de caractères représentée
//    par le buffer pointé par sb.
extern size_t sbuffer_length(sbuffer *sb);

//  sbuffer_clear : vide le buffer pointé par sb.
extern void sbuffer_clear(sbuffer *sb);

//  sbuffer_append : tente d'ajouter le caractère c à la fin de la chaîne de
//    caractères représentée par le buffer pointé par sb. Il n'est pas possible
//    d'insérer un nouveau caractère si le buffer contient le caractère de fin
//    de chaîne '\0'. Renvoie 0 en cas de succès, une valeur non nulle si le
//    buffer contient le caractère de fin de chaîne ou en cas de dépassement de
//    capacité.
extern int sbuffer_append(sbuffer *sb, char c);

//  sbuffer_get_str : si le buffer pointé par sb contient déjà le caractère de
//    fin de chaîne '\0', renvoie la chaîne qu'il contient. Sinon, tente
//    d'ajouter '\0' à la fin du buffer et renvoie NULL en cas de dépassement de
//    capacité; la chaîne si tout s'est bien passé.
extern char *sbuffer_get_str(sbuffer *sb);

#endif
