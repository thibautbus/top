#include "layout_reference.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The file's JSON, parsed into nodes: objects and arrays list their children, a string or a key is kept decoded, a
 * number as a float.  The file is small and the tests short-lived: nothing is freed. */
typedef enum { NODE_OBJECT, NODE_ARRAY, NODE_STRING, NODE_NUMBER, NODE_OTHER } node_kind;
typedef struct node {
    node_kind kind;
    char *key;              /* in an object: this member's name */
    char *string;
    float number;
    struct node *child, *next;
} node;

static node *root;
static const char *where;
static int failures;
static char reference_path[1024];

static void skip(void) { while (*where == ' ' || *where == '\n' || *where == '\r' || *where == '\t') where++; }

static void put_utf8(char **out, unsigned code)
{
    if (code < 0x80u) *(*out)++ = (char)code;
    else if (code < 0x800u) { *(*out)++ = (char)(0xc0u | code >> 6); *(*out)++ = (char)(0x80u | (code & 0x3fu)); }
    else { *(*out)++ = (char)(0xe0u | code >> 12); *(*out)++ = (char)(0x80u | (code >> 6 & 0x3fu)); *(*out)++ = (char)(0x80u | (code & 0x3fu)); }
}

static char *parse_string(void)
{
    if (*where != '"') return NULL;
    const char *start = ++where;
    while (*where && *where != '"') where += *where == '\\' && where[1] ? 2 : 1;
    if (*where != '"') return NULL;
    char *out = malloc((size_t)(where - start) * 2 + 1), *o = out;
    if (!out) return NULL;
    for (const char *p = start; p < where; p++) {
        if (*p != '\\') { *o++ = *p; continue; }
        p++;
        switch (*p) {
            case 'n': *o++ = '\n'; break;
            case 't': *o++ = '\t'; break;
            case 'r': *o++ = '\r'; break;
            case 'b': *o++ = '\b'; break;
            case 'f': *o++ = '\f'; break;
            case 'u': {
                unsigned code = 0;
                for (int i = 1; i <= 4 && p[i]; i++) code = code * 16u + (unsigned)(p[i] <= '9' ? p[i] - '0' : (p[i] | 0x20) - 'a' + 10);
                put_utf8(&o, code);
                p += 4;
                break;
            }
            default: *o++ = *p; break;   /* \" \\ \/ */
        }
    }
    *o = 0;
    where++;
    return out;
}

static node *parse_value(void)
{
    skip();
    node *n = calloc(1, sizeof *n);
    if (!n) return NULL;
    if (*where == '{' || *where == '[') {
        const char close = *where == '{' ? '}' : ']';
        n->kind = *where == '{' ? NODE_OBJECT : NODE_ARRAY;
        where++;
        node **tail = &n->child;
        skip();
        while (*where && *where != close) {
            char *key = NULL;
            if (n->kind == NODE_OBJECT) {
                key = parse_string();
                skip();
                if (!key || *where != ':') return NULL;
                where++;
            }
            node *child = parse_value();
            if (!child) return NULL;
            child->key = key;
            *tail = child;
            tail = &child->next;
            skip();
            if (*where == ',') { where++; skip(); }
        }
        if (*where != close) return NULL;
        where++;
    } else if (*where == '"') {
        n->kind = NODE_STRING;
        if (!(n->string = parse_string())) return NULL;
    } else if (*where == '-' || (*where >= '0' && *where <= '9')) {
        char *end;
        n->kind = NODE_NUMBER;
        n->number = strtof(where, &end);
        where = end;
    } else {
        n->kind = NODE_OTHER;   /* true, false, null */
        while (*where >= 'a' && *where <= 'z') where++;
    }
    return n;
}

static const node *member(const node *object, const char *key)
{
    if (!object || object->kind != NODE_OBJECT) return NULL;
    for (const node *c = object->child; c; c = c->next) if (c->key && !strcmp(c->key, key)) return c;
    return NULL;
}

int layout_reference_load(const char *path)
{
    snprintf(reference_path, sizeof reference_path, "%s", path);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "FAIL cannot read the layout reference, %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = size > 0 ? malloc((size_t)size + 1) : NULL;
    const int read = text && fread(text, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    if (!read) { fprintf(stderr, "FAIL cannot read the layout reference, %s\n", path); return 0; }
    text[size] = 0;
    where = text;
    root = parse_value();
    if (!root || root->kind != NODE_OBJECT || !member(root, "frames")) {
        fprintf(stderr, "FAIL %s is not the launcher's layout reference (no \"frames\")\n", path);
        return 0;
    }
    return 1;
}

static void how_to_measure(void)
{
    fprintf(stderr, "     (%s; to change the layout on purpose, update the probe's value in that file)\n", reference_path);
}

float layout_reference(const char *frame, const char *probe, const char *quantity)
{
    const node *section = !strcmp(frame, "texts") ? member(root, "texts") : member(member(root, "frames"), frame);
    const node *p = member(section, probe), *q = member(p, quantity);
    if (q && q->kind == NODE_NUMBER) return q->number;
    fprintf(stderr, "FAIL %s %s: the layout reference has no %s for it\n", frame, probe, quantity);
    how_to_measure();
    failures++;
    return 0.0f;
}

void layout_near(const char *frame, const char *probe, const char *quantity, float actual)
{
    layout_near_within(frame, probe, quantity, actual, LAYOUT_TOLERANCE);
}

void layout_near_within(const char *frame, const char *probe, const char *quantity, float actual, float tolerance)
{
    const int before = failures;
    const float expected = layout_reference(frame, probe, quantity);
    if (failures != before || fabsf(actual - expected) <= tolerance) return;
    fprintf(stderr, "FAIL %s %s, %s: %.3f, the reference %.3f\n", frame, probe, quantity, (double)actual, (double)expected);
    how_to_measure();
    failures++;
}

void layout_line(const char *frame, const char *probe, const OraclesUiLine *line)
{
    layout_near(frame, probe, "x", line->x);
    layout_near(frame, probe, "w", line->w);
    layout_near(frame, probe, "top", line->y);
    layout_near(frame, probe, "h", line->h);
    layout_near(frame, probe, "baseline", line->baseline);
}

void layout_text(const char *frame, const char *probe, const OraclesUiLine *line)
{
    layout_near(frame, probe, "x", line->x);
    layout_near(frame, probe, "w", line->w);
    layout_near(frame, probe, "baseline", line->baseline);
}

void layout_line_top(const char *frame, const char *probe, const OraclesUiLine *line)
{
    layout_near(frame, probe, "x", line->x);
    layout_near(frame, probe, "w", line->w);
    layout_near(frame, probe, "top", line->y);
}

void layout_box(const char *frame, const char *probe, const OraclesUiBox *box)
{
    layout_near(frame, probe, "x", box->x);
    layout_near(frame, probe, "y", box->y);
    layout_near(frame, probe, "w", box->w);
    layout_near(frame, probe, "h", box->h);
}

int layout_failures(void) { return failures; }
void layout_fail(void) { failures++; }
