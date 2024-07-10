#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char uchar;

#define pad_fprintf(level, stream, fmt, ...)   \
    do {                                       \
        fprintf(stream, "%*s", 2 * level, ""); \
        fprintf(stream, fmt, __VA_ARGS__);     \
    } while (0)

// -----------------------------------------------------------------------------
// intscan
// -----------------------------------------------------------------------------

/* Lookup table for digit values. -1==255>=36 -> invalid */
static const uchar table[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,
    9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1,
    -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

// -----------------------------------------------------------------------------
// Minimal XML writer
// -----------------------------------------------------------------------------

#define XML_NODE_CONTENT_INIT_CAPACITY 16

enum xml_content_type {
    XCT_STRING,
    XCT_XML_NODE_LIST,
};

struct xml_node {
    char *tag;
    char *attributes;
    enum xml_content_type type;
    void *content;
    size_t count;
    size_t capacity;
};

struct xml_node *xml_node_create(char *tag, char *attributes,
                                 enum xml_content_type type)
{
    assert(tag && "Tag must not be empty");
    struct xml_node *result = malloc(sizeof(*result));

    size_t tag_len = strlen(tag);
    result->tag = malloc(tag_len + 1);
    memcpy(result->tag, tag, tag_len + 1);

    if (!attributes) {
        result->attributes = NULL;
    } else {
        size_t attr_len = strlen(attributes);
        result->attributes = malloc(attr_len + 1);
        memcpy(result->attributes, attributes, attr_len + 1);
    }

    result->type = type;

    result->content = NULL;
    result->count = 0;
    result->capacity = 0;

    return result;
}

void xml_node_free(struct xml_node *self)
{
    free(self->tag);
    if (self->attributes)
        free(self->attributes);
    if (self->content) {
        switch (self->type) {
        case XCT_XML_NODE_LIST:
            for (size_t i = 0; i < self->count; ++i)
                xml_node_free(((struct xml_node **)self->content)[i]);
        case XCT_STRING:
            free(self->content);
            break;
        default:
            fprintf(
                stderr,
                "Error: Unknown xml_content_type encountered in `xml_node_free`\n");
            exit(1);
        }
    }
}

void xml_node_set_string_content(struct xml_node *self, char *content)
{
    if (self->type != XCT_STRING) {
        fprintf(
            stderr,
            "Error: calling `xml_node_set_string_content()` with a XML node whose `content` is not XCT_STRING\n");
        exit(1);
    }

    size_t len = strlen(content);
    self->content = realloc(self->content, len + 1);
    memcpy(self->content, content, len + 1);
}

void xml_node_append_xml_node_content(struct xml_node *self,
                                      struct xml_node **content, size_t count)
{
    if (self->type != XCT_XML_NODE_LIST) {
        fprintf(
            stderr,
            "Error: calling `xml_node_append()` with a XML node whose `content` is not XCT_XML_NODE_LIST\n");
        exit(1);
    }

    if (self->count + count > self->capacity) {
        if (self->capacity == 0)
            self->capacity = XML_NODE_CONTENT_INIT_CAPACITY;
        while (self->count + count > self->capacity)
            self->capacity *= 2;
        self->content =
            realloc(self->content, self->capacity * sizeof(struct xml_node *));
    }
    memcpy(((struct xml_node **)self->content) + self->count, content,
           count * sizeof(struct xml_node *));
    self->count += count;
}

#define xml_node_append_xml_node_contentv(self, ...)       \
    do {                                                   \
        xml_node_append_xml_node_content(                  \
            self, (struct xml_node *[]){ __VA_ARGS__ },    \
            sizeof((struct xml_node *[]){ __VA_ARGS__ }) / \
                sizeof(struct xml_node *));                \
    } while (0)

void xml_fprintln(int level, FILE *stream, struct xml_node *node)
{
    pad_fprintf(level, stream, "<%s", node->tag);
    if (node->attributes)
        fprintf(stream, " %s", node->attributes);
    fprintf(stream, ">");

    switch (node->type) {
    case XCT_STRING:
        fprintf(stream, "%s</%s>\n", (char *)node->content, node->tag);
        break;
    case XCT_XML_NODE_LIST:
        fprintf(stream, "\n");
        for (size_t i = 0; i < node->count; ++i) {
            struct xml_node *children = ((struct xml_node **)node->content)[i];
            xml_fprintln(level + 1, stream, children);
        }
        pad_fprintf(level, stream, "</%s>\n", node->tag);
        break;
    default:
        fprintf(stderr, "Error: Unsupported xml_content_type: %d\n",
                (int)node->type);
        exit(1);
    }
}

// -----------------------------------------------------------------------------
// itermcolor item
// -----------------------------------------------------------------------------

struct color_dict_srgb {
    double r;
    double g;
    double b;
};

void xml_node_append_color_dict_srgb(struct xml_node *node,
                                     struct color_dict_srgb item)
{
    char red[20];
    char green[20];
    char blue[20];
    sprintf(red, "%.17f", item.r);
    sprintf(green, "%.17f", item.g);
    sprintf(blue, "%.17f", item.b);

    // Red Component
    struct xml_node *rc_k = xml_node_create("key", NULL, XCT_STRING);
    xml_node_set_string_content(rc_k, "Red Component");
    struct xml_node *rc_v = xml_node_create("real", NULL, XCT_STRING);
    xml_node_set_string_content(rc_v, red);

    // Green Component
    struct xml_node *gc_k = xml_node_create("key", NULL, XCT_STRING);
    xml_node_set_string_content(gc_k, "Green Component");
    struct xml_node *gc_v = xml_node_create("real", NULL, XCT_STRING);
    xml_node_set_string_content(gc_v, green);

    // Blue Component
    struct xml_node *bc_k = xml_node_create("key", NULL, XCT_STRING);
    xml_node_set_string_content(bc_k, "Blue Component");
    struct xml_node *bc_v = xml_node_create("real", NULL, XCT_STRING);
    xml_node_set_string_content(bc_v, blue);

    // Alpha Component
    struct xml_node *ac_k = xml_node_create("key", NULL, XCT_STRING);
    xml_node_set_string_content(ac_k, "Alpha Component");
    struct xml_node *ac_v = xml_node_create("integer", NULL, XCT_STRING);
    xml_node_set_string_content(ac_v, "1");

    // Color Space
    struct xml_node *cs_k = xml_node_create("key", NULL, XCT_STRING);
    xml_node_set_string_content(cs_k, "Color Space");
    struct xml_node *cs_v = xml_node_create("string", NULL, XCT_STRING);
    xml_node_set_string_content(cs_v, "sRGB");

    xml_node_append_xml_node_contentv(node, rc_k, rc_v, gc_k, gc_v, bc_k, bc_v,
                                      ac_k, ac_v, cs_k, cs_v);
}

struct itermcolor_item {
    char *key;
    struct color_dict_srgb dict;
};

struct itermcolor_item itermcolor_item_from_string(char *string)
{
    size_t key_len = strcspn(string, ":");
    char *color = strchr(string + key_len, '#');
    if (!color) {
        fprintf(stderr, "Error: %*s missing hex color definition\n",
                (int)key_len, string);
        exit(1);
    } else if (strlen(color) != 7) {
        fprintf(stderr,
                "Error: Hex color '%s' not supported, '#RRGGBB' required\n",
                color);
        exit(1);
    } else {
        for (size_t i = 0; i < 6; ++i) {
            if (table[(uchar)color[i + 1]] == (uchar)-1) {
                fprintf(stderr, "Error: Invalid hex color '%s'\n", color);
                exit(1);
            }
        }
    }

    char *key = malloc(key_len + 1);
    memcpy(key, string, key_len);
    key[key_len] = '\0';

    double r = (16 * table[(uchar)color[1]] + table[(uchar)color[2]]) / 255.0;
    double g = (16 * table[(uchar)color[3]] + table[(uchar)color[4]]) / 255.0;
    double b = (16 * table[(uchar)color[5]] + table[(uchar)color[6]]) / 255.0;

    return (struct itermcolor_item){ .key = key,
                                     .dict = { .r = r, .g = g, .b = b } };
}

void xml_node_append_itermcolor_item(struct xml_node *node,
                                     struct itermcolor_item item)
{
    struct xml_node *key = xml_node_create("key", NULL, XCT_STRING);
    xml_node_set_string_content(key, item.key);

    struct xml_node *dict = xml_node_create("dict", NULL, XCT_XML_NODE_LIST);
    xml_node_append_color_dict_srgb(dict, item.dict);

    xml_node_append_xml_node_contentv(node, key, dict);
}

// -----------------------------------------------------------------------------
// Helper functions
// -----------------------------------------------------------------------------

void print_help_message(char *program_name)
{
    printf(
        "A simple tool that generates .itermcolors file for iTerm2 theming\n");
    printf("\n");
    printf("Usage: %s build <INPUT> <OUTPUT>\n", program_name);
    printf("       %s new <OUTPUT>\n", program_name);
    printf("\n");
    printf("Commands:\n");
    printf("    build   Compile an input file to a .itermcolors file\n");
    printf("    new     Create an input template\n");
}

char *shift_args(int *argc, char ***argv)
{
    assert(*argc > 0);
    char *result = **argv;
    ++(*argv);
    --(*argc);
    return result;
}

// -----------------------------------------------------------------------------
// Commands
// -----------------------------------------------------------------------------

void gi_build(FILE *istream, FILE *ostream)
{
    const char *xml_header = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    const char *doctype_header =
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">";

    struct xml_node *plist =
        xml_node_create("plist", "version=\"1.0\"", XCT_XML_NODE_LIST);
    struct xml_node *dict = xml_node_create("dict", NULL, XCT_XML_NODE_LIST);

    size_t line_count = 0;
    size_t line_max_col = 80;
    char line[line_max_col + 1];
    while (fgets(line, sizeof(line) / sizeof(line[0]), istream)) {
        ++line_count;
        char *line_end = strchr(line, '\n');
        if (line_end) {
            *line_end = '\0';
        } else {
            fprintf(
                stderr,
                "Warning: Line %zu too long, ignoring the characters after column %zu",
                line_count, line_max_col);
            while (fgetc(istream) != '\n')
                continue;
        }
        struct itermcolor_item item = itermcolor_item_from_string(line);
        xml_node_append_itermcolor_item(dict, item);
    }

    xml_node_append_xml_node_contentv(plist, dict);

    fprintf(ostream, "%s\n", xml_header);
    fprintf(ostream, "%s\n", doctype_header);
    xml_fprintln(0, ostream, plist);

    xml_node_free(plist);
    free(plist);
}

void gi_new(FILE *stream)
{
    const char *items[] = {
        "Selected Text Color", "Selection Color",  "Cursor Guide Color",
        "Cursor Text Color",   "Cursor Color",     "Bold Color",
        "Link Color",          "Foreground Color", "Background Color",
        "Ansi 15 Color",       "Ansi 14 Color",    "Ansi 13 Color",
        "Ansi 12 Color",       "Ansi 11 Color",    "Ansi 10 Color",
        "Ansi 9 Color",        "Ansi 8 Color",     "Ansi 7 Color",
        "Ansi 6 Color",        "Ansi 5 Color",     "Ansi 4 Color",
        "Ansi 3 Color",        "Ansi 2 Color",     "Ansi 1 Color",
        "Ansi 0 Color",
    };
    const char *suffixes[] = { "", " (Light)", " (Dark)" };
    for (size_t s = 0; s < sizeof(suffixes) / sizeof(const char *); ++s) {
        for (size_t i = 0; i < sizeof(items) / sizeof(const char *); ++i)
            fprintf(stream, "%s%s: #000000\n", items[i], suffixes[s]);
    }
}

// -----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    char *program_name = shift_args(&argc, &argv);
    if (argc == 0) {
        print_help_message(program_name);
        return 0;
    }

    char *command = shift_args(&argc, &argv);
    if (!strcmp(command, "build")) {
        if (argc != 2) {
            goto failed;
        } else {
            char *input = shift_args(&argc, &argv);
            char *output = shift_args(&argc, &argv);
            FILE *istream = fopen(input, "r");
            FILE *ostream = !strcmp(output, "-") ? stdout : fopen(output, "w+");
            gi_build(istream, ostream);
            fclose(istream);
            if (strcmp(output, "-"))
                fclose(ostream);
        }
    } else if (!strcmp(command, "new")) {
        if (argc != 1) {
            goto failed;
        } else {
            char *output = shift_args(&argc, &argv);
            FILE *stream = !strcmp(output, "-") ? stdout : fopen(output, "w+");
            gi_new(stream);
            if (strcmp(output, "-"))
                fclose(stream);
        }
    } else {
        goto failed;
    }

    return 0;
failed:
    print_help_message(program_name);
    return 1;
}
