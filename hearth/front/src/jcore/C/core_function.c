#include "jcore/H/jcore.h"

/* Core types: ash.lang.function (the functional interfaces lambdas target).
 * Source of truth for these types: the compiler cannot work without them, so they ship
 * inside it instead of in the optional standard library (the ash-stdlib package). */
const JCoreEntry jcore_function[] = {
  { "ash/lang/function/BiFunction",
    "package ash.lang.function;\n"
    "\n"
    "/// Function of two arguments.\n"
    "public interface BiFunction<A, B, R> {\n"
    "    /// Applies the function.\n"
    "    R apply(A a, B b);\n"
    "}\n" },
  { "ash/lang/function/Comparator",
    "package ash.lang.function;\n"
    "\n"
    "/// Defines an order between two values; negative, zero or positive.\n"
    "public interface Comparator<T> {\n"
    "    /// Orders two values: negative, zero or positive.\n"
    "    int compare(T a, T b);\n"
    "}\n" },
  { "ash/lang/function/Consumer",
    "package ash.lang.function;\n"
    "\n"
    "/// Operation on one value with no result.\n"
    "public interface Consumer<T> {\n"
    "    /// Performs the operation.\n"
    "    void accept(T t);\n"
    "}\n" },
  { "ash/lang/function/Function",
    "package ash.lang.function;\n"
    "\n"
    "/// Function of one argument.\n"
    "public interface Function<T, R> {\n"
    "    /// Applies the function.\n"
    "    R apply(T t);\n"
    "}\n" },
  { "ash/lang/function/Predicate",
    "package ash.lang.function;\n"
    "\n"
    "/// Test of one value.\n"
    "public interface Predicate<T> {\n"
    "    /// Evaluates the predicate.\n"
    "    bool test(T t);\n"
    "}\n" },
  { "ash/lang/function/Runnable",
    "package ash.lang.function;\n"
    "\n"
    "/// Task with no arguments and no result.\n"
    "public interface Runnable {\n"
    "    /// Performs the task.\n"
    "    void run();\n"
    "}\n" },
  { "ash/lang/function/Supplier",
    "package ash.lang.function;\n"
    "\n"
    "/// Source of values with no arguments.\n"
    "public interface Supplier<T> {\n"
    "    /// Element at the index (or the stored value).\n"
    "    T get();\n"
    "}\n" },
  { 0, 0 }
};
