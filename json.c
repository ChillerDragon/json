#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
  TYPE_UNDEFINED,
  TYPE_NULL,
  TYPE_BOOL,
  TYPE_STR,
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_OBJ,
  TYPE_ARR,
};

const char *type_to_str(int type) {
  if (type == TYPE_UNDEFINED) {
    return "UNDEFINED";
  }
  if (type == TYPE_NULL) {
    return "NULL";
  }
  if (type == TYPE_BOOL) {
    return "BOOL";
  }
  if (type == TYPE_STR) {
    return "STRING";
  }
  if (type == TYPE_INT) {
    return "INTEGER";
  }
  if (type == TYPE_FLOAT) {
    return "FLOAT";
  }
  if (type == TYPE_OBJ) {
    return "OBJECT";
  }
  if (type == TYPE_ARR) {
    return "ARRAY";
  }
  return "unknown";
}

enum {
  ARR_STATE_NONE,
  ARR_STATE_GOT_COMMA, // expect value or end
};

enum {
  OBJ_STATE_NONE,      // not a object
  OBJ_STATE_PRE_KEY,   // expect "
  OBJ_STATE_KEY,       // expect key char or "
  OBJ_STATE_KEY_END,   // expect :
  OBJ_STATE_VALUE_END, // expect }
};

// {        "foo":              2212              }
//  ^       ^   ^^              ^   ^
//  pre key |   |pre value      |   value end
//          key |              value
//             key end

// {        "foo":              "xxxx"              }
//  ^       ^   ^^              ^    ^
//  pre key |   |pre value      |    value end
//          key |              value
//             key end

struct JsonValue {
  int type;

  int integer;
  char *str;
  int length;
  char **keys;
  void **values;
} typedef JsonValue;

void _panic(const char *msg, const char *file, int line) {
  fprintf(stderr, "%s:%d JSON CORE PANIC!!! %s\n", file, line, msg);
  exit(1);
}

#define PANIC(msg) _panic(msg, __FILE__, __LINE__);

void _assert_int_eq(int expected, int actual, const char *file, int line) {
  if (expected == actual)
    return;

  fprintf(stderr, "%s:%d assert failed! expected: %d got: %d\n", file, line,
          expected, actual);
  exit(1);
}

#define ASSERT_INT_EQ(expected, actual)                                        \
  _assert_int_eq(expected, actual, __FILE__, __LINE__);

void _assert_str_eq(const char *expected, const char *actual, const char *file,
                    int line) {
  if (!strcmp(expected, actual))
    return;

  fprintf(stderr, "%s:%d assert failed! expected: \"%s\" got: \"%s\"\n", file,
          line, expected, actual);
  exit(1);
}

#define ASSERT_STR_EQ(expected, actual)                                        \
  _assert_str_eq(expected, actual, __FILE__, __LINE__);

// returns 0 on error
// returns number of consumed characters on success
int json_parse(JsonValue *out, const char *str) {
  out->type = TYPE_UNDEFINED;
  out->str = NULL;
  out->length = 0;
  out->integer = 0;
  out->keys = NULL;
  out->values = NULL;
  void *complex_value = NULL;
  int obj_state = OBJ_STATE_NONE;
  int arr_state = ARR_STATE_NONE;
  int i = 0;
  // TODO: allow bigger numbers
  //       max integer digits is 16
  // TODO: dont 0 initialize it its slow af but im too lazy to nullterm in dbg
  //       msgs
  // TODO: support strings longer than 16 LMAO
  char buf[16] = {0};
  // buf index
  int b = 0;
  while (1) {
    if (str[i] == 0) {
      switch (out->type) {
      case TYPE_UNDEFINED:
        PANIC("unexpected eof for undefined type");
        break;
      case TYPE_INT:
        goto end_int;
        break;
      case TYPE_STR:
        PANIC("missing \"");
        break;
      default:
        PANIC("unhandled type on eof");
      }
      PANIC("EOF");
    }

    switch (out->type) {
    case TYPE_UNDEFINED:
      switch (str[i]) {
      case ' ':
      case '\n':
      case '\t':
        // skip to values
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        out->type = TYPE_INT;
        buf[b++] = str[i];
        break;
      case '"':
        out->type = TYPE_STR;
        break;
      case '{':
        out->type = TYPE_OBJ;
        obj_state = OBJ_STATE_PRE_KEY;
        break;
      case '[':
        // puts("got array");
        out->type = TYPE_ARR;
        break;
      default:
        fprintf(stderr, "i=%d type=%s str=%s\n", i, type_to_str(out->type),
                str + i);
        PANIC("unexpected symbol");
      }
      break;
    case TYPE_INT:
      switch (str[i]) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        buf[b++] = str[i];
        // printf("consume int buf=%s\n", buf);
        break;
      default:
        goto end_int;
      }
      break;
    case TYPE_STR:
      if (str[i] == '"') {
        buf[b] = 0;
        int len = strlen(buf) + 1;
        out->str = malloc(len);
        strncpy(out->str, buf, len);
        return i + 1;
      }
      buf[b++] = str[i];
      // printf("consume str buf=%s\n", buf);
      break;
    case TYPE_ARR:
      if (str[i] == ']') {
        // TODO: do we need to do anything here? xd
        return i;
      }

      // skip to value
      if (str[i] == ' ')
        break;
      if (str[i] == '\n')
        break;
      if (str[i] == '\t')
        break;
      if (str[i] == ',') {
        if (arr_state == ARR_STATE_GOT_COMMA) {
          PANIC("got array with two commas");
        }
        arr_state = ARR_STATE_GOT_COMMA;
        break;
      }
      arr_state = ARR_STATE_NONE;

      out->length++;
      if (!(out->keys = realloc(out->keys, sizeof(char *) * out->length))) {
        fprintf(stderr, "errno: %d\n", errno);
        PANIC("array realloc keys failed");
      }
      if (!(out->values = realloc(out->values, sizeof(void *) * out->length))) {
        fprintf(stderr, "errno: %d\n", errno);
        PANIC("array realloc values failed");
      }

      snprintf(buf, sizeof(buf), "%d", out->length);
      int len = strlen(buf) + 1;
      out->keys[out->length - 1] = malloc(len);
      strncpy(out->keys[out->length - 1], buf, len);
      JsonValue *v = malloc(sizeof(JsonValue));
      // printf("got array value=%s\n", str + i);
      i += json_parse(v, str + i) - 1;
      // printf("after array value=%s\n", str + i);
      out->values[out->length - 1] = v;
      break;
    case TYPE_OBJ:
      if (str[i] == '}') {
        // TODO: do we need to do anything here? xd
        return i + 1;
      }
      // consume obj

      switch (obj_state) {
      case OBJ_STATE_PRE_KEY:
        switch (str[i]) {
        case ' ':
        case '\n':
        case '\t':
          // skip to key
          break;
        case '"':
          obj_state = OBJ_STATE_KEY;
          break;
        default:
          PANIC("unexpected character")
        }
        break;
      case OBJ_STATE_KEY:
        if (str[i] == '"') {
          obj_state = OBJ_STATE_KEY_END;

          out->length++;
          if (!(out->keys = realloc(out->keys, sizeof(char *) * out->length))) {
            fprintf(stderr, "errno: %d\n", errno);
            PANIC("realloc keys failed");
          }
          if (!(out->values =
                    realloc(out->values, sizeof(void *) * out->length))) {
            fprintf(stderr, "errno: %d\n", errno);
            PANIC("realloc values failed");
          }

          buf[b] = 0;
          int len = strlen(buf) + 1;
          out->keys[out->length - 1] = malloc(len);
          out->values[out->length - 1] = NULL;
          // printf("set key numvalues=%d\n", out->length);
          strncpy(out->keys[out->length - 1], buf, len);
          // printf("set keys[%d] = %s\n", out->length - 1,
          // out->keys[out->length - 1]);
          break;
        }
        buf[b++] = str[i];
        break;
      case OBJ_STATE_KEY_END:
        if (str[i] != ':') {
          fprintf(stderr, "at i=%d got str=%s\n", i, str + i);
          PANIC("expected :");
        }

        JsonValue *v = malloc(sizeof(JsonValue));
        i += json_parse(v, str + i + 1);
        out->values[out->length - 1] = v;
        obj_state = OBJ_STATE_VALUE_END;
        break;
      case OBJ_STATE_VALUE_END:

        switch (str[i]) {
        case ' ':
        case '\n':
        case '\t':
          // skip to }
          break;
        case '}':
          return i;
          break;
        case ',':
          b = 0;
          memset(buf, 0, sizeof(buf));
          obj_state = OBJ_STATE_PRE_KEY;
          break;
        default:
          fprintf(stderr, "i=%d str=%s\n", i, str + i);
          PANIC("expected }")
        }
        break;
      default:
        PANIC("unhandled object state");
      }

      break;
    default:
      PANIC("unhandled type");
    }

    i++;
  }

end_int:
  buf[b] = 0;
  out->integer = atoi(buf);
  return i;
}

void json_free(JsonValue *v) {
  if (v->str) {
    free(v->str);
    v->str = NULL;
  }
  for (int i = 0; i < v->length; i++) {
    if (v->keys[i]) {
      // printf("freeing key[%d]\n", i);
      free(v->keys[i]);
      v->keys[i] = NULL;
    }
    if (v->values[i]) {
      // printf("freeing value[%d]\n", i);
      free(v->values[i]);
      v->values[i] = NULL;
    }
  }
  if (v->length) {
    // puts("freeing keys");
    free(v->keys);
    // puts("freeing values");
    free(v->values);
    v->keys = NULL;
    v->values = NULL;
  }
}

int main() {
  JsonValue v;

  json_parse(&v, "1");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(1, v.integer);
  json_free(&v);

  json_parse(&v, "               1");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(1, v.integer);
  json_free(&v);

  json_parse(&v, "2");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(2, v.integer);
  json_free(&v);

  json_parse(&v, "   1823          ,");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(1823, v.integer);
  json_free(&v);

  json_parse(&v, "2}");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(2, v.integer);
  json_free(&v);

  json_parse(&v, "2]");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(2, v.integer);
  json_free(&v);

  int n = json_parse(&v, "2223   }");
  ASSERT_INT_EQ(4, n);
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(2223, v.integer);
  json_free(&v);

  json_parse(&v, "59");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(59, v.integer);
  json_free(&v);

  json_parse(&v, "\"a\"");
  printf("value: %s\n", v.str);
  ASSERT_STR_EQ("a", v.str);
  json_free(&v);

  json_parse(&v, "\"foo\"");
  printf("value: %s\n", v.str);
  ASSERT_STR_EQ("foo", v.str);
  json_free(&v);

  json_parse(&v, "{\"a\": 2}");
  printf("key: %s\n", v.keys[0]);
  ASSERT_STR_EQ("a", v.keys[0]);
  json_free(&v);

  json_parse(&v, "{\"foo\": 2}");
  printf("key: %s\n", v.keys[0]);
  ASSERT_STR_EQ("foo", v.keys[0]);
  printf("value type: %s\n", type_to_str(((JsonValue *)v.values[0])->type));
  printf("value: %d\n", (((JsonValue *)v.values[0])->integer));
  ASSERT_INT_EQ(2, (((JsonValue *)v.values[0])->integer));
  json_free(&v);

  json_parse(&v, "{\"foo\": 2, \"bar\": 420}");
  // foo
  printf("key: %s\n", v.keys[0]);
  ASSERT_STR_EQ("foo", v.keys[0]);
  printf("value type: %s\n", type_to_str(((JsonValue *)v.values[0])->type));
  printf("value: %d\n", (((JsonValue *)v.values[0])->integer));
  ASSERT_INT_EQ(2, (((JsonValue *)v.values[0])->integer));
  // bar
  printf("key: %s\n", v.keys[1]);
  ASSERT_STR_EQ("bar", v.keys[1]);
  printf("value type: %s\n", type_to_str(((JsonValue *)v.values[1])->type));
  printf("value: %d\n", (((JsonValue *)v.values[1])->integer));
  ASSERT_INT_EQ(420, (((JsonValue *)v.values[1])->integer));
  json_free(&v);

  json_parse(&v, "{\"foo\": {\"bar\": 2}}");
  printf("key: %s\n", v.keys[0]);
  ASSERT_STR_EQ("foo", v.keys[0]);
  printf("value type: %s\n", type_to_str(((JsonValue *)v.values[0])->type));
  ASSERT_STR_EQ("OBJECT", type_to_str(((JsonValue *)v.values[0])->type));
  printf("value[0].keys[0] %d\n",
         ((JsonValue *)((JsonValue *)v.values[0])->values[0])->integer);
  ASSERT_INT_EQ(2,
                ((JsonValue *)((JsonValue *)v.values[0])->values[0])->integer);
  json_free(&v);

  json_parse(&v, "{}");
  ASSERT_STR_EQ("OBJECT", type_to_str(v.type));
  ASSERT_INT_EQ(0, v.length);
  json_free(&v);

  json_parse(&v, "[]");
  ASSERT_STR_EQ("ARRAY", type_to_str(v.type));
  ASSERT_INT_EQ(0, v.length);
  json_free(&v);

  json_parse(&v, "[                          ]");
  ASSERT_INT_EQ(0, v.length);
  json_free(&v);

  json_parse(&v, "[2]");
  ASSERT_INT_EQ(1, v.length);
  ASSERT_INT_EQ(2, (((JsonValue *)v.values[0])->integer));
  json_free(&v);

  json_parse(&v, "[   625        ]");
  ASSERT_INT_EQ(1, v.length);
  ASSERT_INT_EQ(625, (((JsonValue *)v.values[0])->integer));
  json_free(&v);

  json_parse(&v, "[   625    , 2    ]");
  ASSERT_INT_EQ(2, v.length);
  ASSERT_INT_EQ(625, (((JsonValue *)v.values[0])->integer));
  ASSERT_INT_EQ(2, (((JsonValue *)v.values[1])->integer));
  json_free(&v);

  json_parse(&v, "[1,2,3]");
  ASSERT_INT_EQ(3, v.length);
  ASSERT_INT_EQ(1, (((JsonValue *)v.values[0])->integer));
  ASSERT_INT_EQ(2, (((JsonValue *)v.values[1])->integer));
  ASSERT_INT_EQ(3, (((JsonValue *)v.values[2])->integer));
  json_free(&v);

  json_parse(&v, "[1,2,[3]]");
  ASSERT_INT_EQ(3, v.length);
  ASSERT_INT_EQ(1, (((JsonValue *)v.values[0])->integer));
  ASSERT_INT_EQ(2, (((JsonValue *)v.values[1])->integer));
  ASSERT_STR_EQ("ARRAY", type_to_str(((JsonValue *)v.values[2])->type));
  ASSERT_INT_EQ(3,
                ((JsonValue *)((JsonValue *)v.values[2])->values[0])->integer);
  json_free(&v);

  json_parse(&v, "[{}]");
  ASSERT_INT_EQ(1, v.length);
  ASSERT_STR_EQ("ARRAY", type_to_str(v.type));
  ASSERT_STR_EQ("OBJECT", type_to_str(((JsonValue *)v.values[0])->type));
  json_free(&v);

  json_parse(&v, "[[[[]]]]");
  ASSERT_INT_EQ(1, v.length);
  ASSERT_STR_EQ("ARRAY", type_to_str(v.type));
  ASSERT_STR_EQ("ARRAY", type_to_str(((JsonValue *)v.values[0])->type));
  ASSERT_STR_EQ(
      "ARRAY",
      type_to_str(((JsonValue *)((JsonValue *)v.values[0])->values[0])->type));
  ASSERT_STR_EQ(
      "ARRAY",
      type_to_str(
          ((JsonValue *)((JsonValue *)((JsonValue *)v.values[0])->values[0])
               ->values[0])
              ->type));
  json_free(&v);

  json_parse(&v, "{\"foo\": \"bar\"}");
  ASSERT_INT_EQ(1, v.length);
  ASSERT_STR_EQ("OBJECT", type_to_str(v.type));
  ASSERT_STR_EQ("STRING", type_to_str(((JsonValue *)v.values[0])->type));
  ASSERT_STR_EQ("bar", ((JsonValue *)v.values[0])->str);
  json_free(&v);

  json_parse(&v, "[{\"foo\": \"bar\"}]");
  ASSERT_INT_EQ(1, v.length);
  ASSERT_STR_EQ("ARRAY", type_to_str(v.type));
  ASSERT_STR_EQ("OBJECT", type_to_str(((JsonValue *)v.values[0])->type));
  json_free(&v);

  json_parse(&v, "[1,2,[3, {\"foo\": \"bar\"}]]");
  ASSERT_INT_EQ(3, v.length);
  ASSERT_INT_EQ(1, (((JsonValue *)v.values[0])->integer));
  ASSERT_INT_EQ(2, (((JsonValue *)v.values[1])->integer));
  ASSERT_STR_EQ("ARRAY", type_to_str(((JsonValue *)v.values[2])->type));
  ASSERT_INT_EQ(3,
                ((JsonValue *)((JsonValue *)v.values[2])->values[0])->integer);
  ASSERT_STR_EQ(
      "OBJECT",
      type_to_str(((JsonValue *)((JsonValue *)v.values[2])->values[1])->type));
  ASSERT_STR_EQ("foo",
                ((JsonValue *)((JsonValue *)v.values[2])->values[1])->keys[0]);
  ASSERT_STR_EQ(
      "bar", ((JsonValue *)((JsonValue *)((JsonValue *)v.values[2])->values[1])
                  ->values[0])
                 ->str);
  json_free(&v);
};
