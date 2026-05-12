//> A Virtual Machine vm-c
//> Types of Values include-stdarg
#include <stdarg.h>
//< Types of Values include-stdarg
//> vm-include-stdio
#include <stdio.h>
//> Strings vm-include-string
#include <string.h>
//< Strings vm-include-string
//> Calls and Functions vm-include-time
#include <time.h>
//< Calls and Functions vm-include-time
#include <math.h>

//< vm-include-stdio
#include "common.h"
//> Scanning on Demand vm-include-compiler
#include "compiler.h"
//< Scanning on Demand vm-include-compiler
//> vm-include-debug
#include "debug.h"
//< vm-include-debug
//> Strings vm-include-object-memory
#include "object.h"
#include "memory.h"
//< Strings vm-include-object-memory
#include "vm.h"

VM vm; // [one]
//> Forward declarations for native functions
static void runtimeError(const char* format, ...);
//< Forward declarations for native functions
//> Calls and Functions clock-native
static bool clockNative(int argCount, Value* args, Value* result) {
  *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
  return true;
}
//< Calls and Functions clock-native
//> sqrt-native
static bool sqrtNative(int argCount, Value* args, Value* result) {
  if (!IS_NUMBER(args[0])) {
    runtimeError("sqrt() argument must be a number.");
    return false;
  }

  double x = AS_NUMBER(args[0]);
  if (x < 0) {
    runtimeError("sqrt() argument must be non-negative.");
    return false;
  }

  *result = NUMBER_VAL(sqrt(x));
  return true;
}
//< sqrt-native
//> abs-native
static bool absNative(int argCount, Value* args, Value* result) {
  if (!IS_NUMBER(args[0])) {
    runtimeError("abs() argument must be a number.");
    return false;
  }

  double x = AS_NUMBER(args[0]);
  *result = NUMBER_VAL(fabs(x));
  return true;
}
//< abs-native
//> min-native
static bool minNative(int argCount, Value* args, Value* result) {
  if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
    runtimeError("min() arguments must be numbers.");
    return false;
  }

  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  *result = NUMBER_VAL(a < b ? a : b);
  return true;
}
//< min-native
//> max-native
static bool maxNative(int argCount, Value* args, Value* result) {
  if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
    runtimeError("max() arguments must be numbers.");
    return false;
  }

  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  *result = NUMBER_VAL(a > b ? a : b);
  return true;
}
//< max-native
//> len-native
static bool lenNative(int argCount, Value* args, Value* result) {
  if (!IS_STRING(args[0])) {
    runtimeError("len() argument must be a string.");
    return false;
  }

  ObjString* string = AS_STRING(args[0]);
  *result = NUMBER_VAL(string->length);
  return true;
}
//< len-native
//> type-native
static bool typeNative(int argCount, Value* args, Value* result) {
  const char* typeName;

  if (IS_BOOL(args[0])) {
    typeName = "bool";
  } else if (IS_NIL(args[0])) {
    typeName = "nil";
  } else if (IS_NUMBER(args[0])) {
    typeName = "number";
  } else if (IS_OBJ(args[0])) {
    switch (OBJ_TYPE(args[0])) {
      case OBJ_STRING:   typeName = "string"; break;
      case OBJ_FUNCTION: typeName = "function"; break;
      case OBJ_NATIVE:   typeName = "native"; break;
      case OBJ_CLOSURE:  typeName = "closure"; break;
      case OBJ_CLASS:    typeName = "class"; break;
      case OBJ_INSTANCE: typeName = "instance"; break;
      case OBJ_BOUND_METHOD: typeName = "bound_method"; break;
      case OBJ_UPVALUE:  typeName = "upvalue"; break;
      default:           typeName = "object"; break;
    }
  } else {
    typeName = "unknown";
  }

  *result = OBJ_VAL(copyString(typeName, (int)strlen(typeName)));
  return true;
}
//< type-native
//> has-field-native
static bool hasFieldNative(int argCount, Value* args, Value* result) {
  if (argCount != 2) {
    runtimeError("hasField() expects 2 arguments.");
    return false;
  }

  if (!IS_INSTANCE(args[0])) {
    runtimeError("First argument must be an instance.");
    return false;
  }

  if (!IS_STRING(args[1])) {
    runtimeError("Second argument must be a string.");
    return false;
  }

  ObjInstance* instance = AS_INSTANCE(args[0]);
  ObjString* name = AS_STRING(args[1]);

  Value value;
  *result = BOOL_VAL(tableGet(&instance->fields, name, &value));
  return true;
}
//< has-field-native
//> get-field-native
static bool getFieldNative(int argCount, Value* args, Value* result) {
  if (argCount != 2) {
    runtimeError("getField() expects 2 arguments.");
    return false;
  }

  if (!IS_INSTANCE(args[0])) {
    runtimeError("First argument must be an instance.");
    return false;
  }

  if (!IS_STRING(args[1])) {
    runtimeError("Second argument must be a string.");
    return false;
  }

  ObjInstance* instance = AS_INSTANCE(args[0]);
  ObjString* name = AS_STRING(args[1]);

  Value value;
  if (!tableGet(&instance->fields, name, &value)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }

  *result = value;
  return true;
}
//< get-field-native
//> set-field-native
static bool setFieldNative(int argCount, Value* args, Value* result) {
  if (argCount != 3) {
    runtimeError("setField() expects 3 arguments.");
    return false;
  }

  if (!IS_INSTANCE(args[0])) {
    runtimeError("First argument must be an instance.");
    return false;
  }

  if (!IS_STRING(args[1])) {
    runtimeError("Second argument must be a string.");
    return false;
  }

  ObjInstance* instance = AS_INSTANCE(args[0]);
  ObjString* name = AS_STRING(args[1]);

  tableSet(&instance->fields, name, args[2]);
  *result = args[2];
  return true;
}
//< set-field-native
//> delete-field-native
static bool deleteFieldNative(int argCount, Value* args, Value* result) {
  if (argCount != 2) {
    runtimeError("deleteField() expects 2 arguments.");
    return false;
  }

  if (!IS_INSTANCE(args[0])) {
    runtimeError("First argument must be an instance.");
    return false;
  }

  if (!IS_STRING(args[1])) {
    runtimeError("Second argument must be a string.");
    return false;
  }

  ObjInstance* instance = AS_INSTANCE(args[0]);
  ObjString* name = AS_STRING(args[1]);

  *result = BOOL_VAL(tableDelete(&instance->fields, name));
  return true;
}
//< delete-field-native
//> reset-stack
static void resetStack() {
  vm.stackTop = vm.stack;
//> Calls and Functions reset-frame-count
  vm.frameCount = 0;
//< Calls and Functions reset-frame-count
//> Closures init-open-upvalues
  vm.openUpvalues = NULL;
//< Closures init-open-upvalues
}
//< reset-stack
//> Types of Values runtime-error
static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

/* Types of Values runtime-error < Calls and Functions runtime-error-temp
  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];
*/
/* Calls and Functions runtime-error-temp < Calls and Functions runtime-error-stack
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  size_t instruction = frame->ip - frame->function->chunk.code - 1;
  int line = frame->function->chunk.lines[instruction];
*/
/* Types of Values runtime-error < Calls and Functions runtime-error-stack
  fprintf(stderr, "[line %d] in script\n", line);
*/
//> Calls and Functions runtime-error-stack
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
/* Calls and Functions runtime-error-stack < Closures runtime-error-function
    ObjFunction* function = frame->function;
*/
//> Closures runtime-error-function
    ObjFunction* function = frame->closure->function;
//< Closures runtime-error-function
    fprintf(stderr, "[line %d] in ",
            function->chunk.lineEntries ? function->chunk.lineEntries[0].line : 0);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

//< Calls and Functions runtime-error-stack
  resetStack();
}
//< Types of Values runtime-error
//> Calls and Functions define-native
static void defineNative(const char* name, NativeFn function, int arity) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function, arity)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}
//< Calls and Functions define-native

void initVM() {
//> call-reset-stack
  vm.stack = ALLOCATE(Value, STACK_MAX);
  vm.stackCapacity = STACK_MAX;
  resetStack();
//< call-reset-stack
//> Strings init-objects-root
  vm.objects = NULL;
//< Strings init-objects-root
//> Garbage Collection init-gc-fields
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;
//< Garbage Collection init-gc-fields
//> Garbage Collection init-gray-stack

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;
//< Garbage Collection init-gray-stack
//> Global Variables init-globals

  initTable(&vm.globals);
//< Global Variables init-globals
//> Hash Tables init-strings
  initTable(&vm.strings);
//< Hash Tables init-strings
//> Methods and Initializers init-init-string

//> null-init-string
  vm.initString = NULL;
//< null-init-string
  vm.initString = copyString("init", 4);
//< Methods and Initializers init-init-string
//> Calls and Functions define-native-clock

  defineNative("clock", clockNative, 0);
  defineNative("sqrt", sqrtNative, 1);
  defineNative("abs", absNative, 1);
  defineNative("min", minNative, 2);
  defineNative("max", maxNative, 2);
  defineNative("len", lenNative, 1);
  defineNative("type", typeNative, 1);
  defineNative("hasField", hasFieldNative, 2);
  defineNative("getField", getFieldNative, 2);
  defineNative("setField", setFieldNative, 3);
  defineNative("deleteField", deleteFieldNative, 2);
//< Calls and Functions define-native-clock
}

void freeVM() {
//> Global Variables free-globals
  freeTable(&vm.globals);
//< Global Variables free-globals
//> Hash Tables free-strings
  freeTable(&vm.strings);
//< Hash Tables free-strings
//> Methods and Initializers clear-init-string
  vm.initString = NULL;
//< Methods and Initializers clear-init-string
//> Strings call-free-objects
  freeObjects();
//< Strings call-free-objects
  FREE_ARRAY(Value, vm.stack, vm.stackCapacity);
}
//> push
void push(Value value) {
  if (IS_OBJ(value)) incRef(AS_OBJ(value));
  *vm.stackTop = value;
  vm.stackTop++;
}
//< push
//> pop
Value pop() {
  vm.stackTop--;
  Value value = *vm.stackTop;
  if (IS_OBJ(value)) decRef(AS_OBJ(value));
  return value;
}
//< pop
//> Types of Values peek
static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}
//< Types of Values peek
//> Calls and Functions call
static bool callFunction(ObjFunction* function, int argCount) {
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d.",
        function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->closure = NULL;
  frame->ip = function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callClosure(ObjClosure* closure, int argCount) {
  ObjFunction* function = closure->function;

  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d.",
        function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->closure = closure;
  frame->ip = function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}
//< Calls and Functions call
//> Calls and Functions call-value
static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD:
        vm.stackTop[-argCount - 1] = AS_BOUND_METHOD(callee)->receiver;
        return callClosure(AS_BOUND_METHOD(callee)->method, argCount);

      case OBJ_CLASS: {
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(AS_CLASS(callee)));
        return true;
      }

      case OBJ_CLOSURE:
        return callClosure(AS_CLOSURE(callee), argCount);

      case OBJ_FUNCTION:
        return callFunction(AS_FUNCTION(callee), argCount);

      case OBJ_NATIVE: {
        ObjNative* native = AS_NATIVE_OBJ(callee);

        if (argCount != native->arity) {
          runtimeError("Expected %d arguments but got %d.",
              native->arity, argCount);
          return false;
        }

        Value result;
        if (!native->function(argCount, vm.stackTop - argCount, &result)) {
          return false;
        }

        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }

      default:
        break;
    }
  }

  runtimeError("Can only call functions and classes.");
  return false;
}
//< Calls and Functions call-value
//> Methods and Initializers invoke-from-class
static bool invokeFromClass(ObjClass* klass, ObjString* name,
                            int argCount) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }
  return callClosure(AS_CLOSURE(method), argCount);
}
//< Methods and Initializers invoke-from-class
//> Methods and Initializers invoke
static bool invoke(ObjString* name, int argCount) {
  Value receiver = peek(argCount);
//> invoke-check-type

  if (!IS_INSTANCE(receiver)) {
    runtimeError("Only instances have methods.");
    return false;
  }

//< invoke-check-type
  ObjInstance* instance = AS_INSTANCE(receiver);
//> invoke-field

  Value value;
  if (tableGet(&instance->fields, name, &value)) {
    vm.stackTop[-argCount - 1] = value;
    return callValue(value, argCount);
  }

//< invoke-field
  return invokeFromClass(instance->klass, name, argCount);
}
//< Methods and Initializers invoke
//> Methods and Initializers bind-method
static bool bindMethod(ObjClass* klass, ObjString* name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError("Undefined property '%s'.", name->chars);
    return false;
  }

  ObjBoundMethod* bound = newBoundMethod(peek(0),
                                         AS_CLOSURE(method));
  pop();
  push(OBJ_VAL(bound));
  return true;
}
//< Methods and Initializers bind-method
//> Closures capture-upvalue
static ObjUpvalue* captureUpvalue(Value* local) {
//> look-for-existing-upvalue
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

//< look-for-existing-upvalue
  ObjUpvalue* createdUpvalue = newUpvalue(local);
//> insert-upvalue-in-list
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

//< insert-upvalue-in-list
  return createdUpvalue;
}
//< Closures capture-upvalue
//> Closures close-upvalues
static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}
//< Closures close-upvalues
//> Methods and Initializers define-method
static void defineMethod(ObjString* name) {
  Value method = peek(0);
  ObjClass* klass = AS_CLASS(peek(1));
  tableSet(&klass->methods, name, method);
  pop();
}
//< Methods and Initializers define-method
//> Types of Values is-falsey
static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}
//< Types of Values is-falsey
//> Strings concatenate
static void concatenate() {
/* Strings concatenate < Garbage Collection concatenate-peek
  ObjString* b = AS_STRING(pop());
  ObjString* a = AS_STRING(pop());
*/
//> Garbage Collection concatenate-peek
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));
//< Garbage Collection concatenate-peek

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
//> Garbage Collection concatenate-pop
  pop();
  pop();
//< Garbage Collection concatenate-pop
  push(OBJ_VAL(result));
}
//< Strings concatenate
//> run
static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  register uint8_t* ip = frame->ip;

  #define READ_BYTE() (*ip++)
  #define READ_SHORT() \
    (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
  #define READ_CONSTANT() \
    (frame->function->chunk.constants.values[READ_BYTE()])
  #define READ_STRING() AS_STRING(READ_CONSTANT())
  #define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    frame->ip = ip;
    disassembleInstruction(&frame->function->chunk,
        (int)(ip - frame->function->chunk.code));

    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }

      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_POP: pop(); break;

      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }

      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }

      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }

      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }

      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }

      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }

      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }

      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }

      case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS: BINARY_OP(BOOL_VAL, <); break;

      case OP_ADD:
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          runtimeError("Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;

      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;

      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;

      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      }

      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        ip += offset;
        break;
      }

      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) ip += offset;
        break;
      }

      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        ip -= offset;
        break;
      }

      case OP_CALL: {
        int argCount = READ_BYTE();

        frame->ip = ip;
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }

      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());

        if (function->upvalueCount == 0) {
          push(OBJ_VAL(function));
        } else {
          ObjClosure* closure = newClosure(function);
          push(OBJ_VAL(closure));

          for (int i = 0; i < closure->upvalueCount; i++) {
            uint8_t isLocal = READ_BYTE();
            uint8_t index = READ_BYTE();
            if (isLocal) {
              closure->upvalues[i] = captureUpvalue(frame->slots + index);
            } else {
              closure->upvalues[i] = frame->closure->upvalues[index];
            }
          }
        }
        break;
      }

      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;

      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        vm.frameCount--;

        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);

        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
    }
  }

  #undef READ_BYTE
  #undef READ_SHORT
  #undef READ_CONSTANT
  #undef READ_STRING
  #undef BINARY_OP
}
//< run
//> omit
void hack(bool b) {
  // Hack to avoid unused function error. run() is not used in the
  // scanning chapter.
  run();
  if (b) hack(false);
}
//< omit
//> interpret
/* A Virtual Machine interpret < Scanning on Demand vm-interpret-c
InterpretResult interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
*/
//> Scanning on Demand vm-interpret-c
InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  callFunction(function, 0);

  return run();
}
//< interpret
