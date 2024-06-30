#include <stdio.h>
#include <stdlib.h>

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

struct JsonValue {
  int type;

  int integer;
  const char *str;
  const char **keys;
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

// returns 0 on error
// returns 1 on succes
int parse_value(JsonValue *out, const char *str) {
  out->type = TYPE_UNDEFINED;
  int i = 0;
  // TODO: allow bigger numbers
  //       max integer digits is 16
  // TODO: dont 0 initialize it its slow af but im too lazy to nullterm in dbg
  // msgs
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
        buf[b] = 0;
        out->integer = atoi(buf);
        return 1;
        break;
      default:
        PANIC("unhandled type on eof");
      }
      PANIC("EOF");
    }

    switch (out->type) {
    case TYPE_UNDEFINED:
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
        out->type = TYPE_INT;
        buf[b++] = str[i];
        break;
      case '"':
        out->type = TYPE_STR;
        buf[b++] = str[i];
        break;
      default:
        PANIC("unexpected symbol");
      }
      break;
    case TYPE_INT:
      buf[b++] = str[i];
      break;
    default:
      PANIC("unhandled type");
    }

    i++;
  }
}

int main() {
  JsonValue v;

  parse_value(&v, "1");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(1, v.integer);

  parse_value(&v, "2");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(2, v.integer);

  parse_value(&v, "59");
  printf("value: %d\n", v.integer);
  ASSERT_INT_EQ(59, v.integer);
};
