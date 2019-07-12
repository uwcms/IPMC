# Coding Style
The coding style used in the framework is based on Google's CPP styling
([link](https://google.github.io/styleguide/cppguide.html#Naming)) with a few changes:

## Class Data Members

Data members of classes, both static and non-static, are named like ordinary nonmember variables.

```c++
class TableInfo {
  ...
 private:
  string table_name;  // OK
  string tablename;   // OK.
  static Pool<TableInfo>* pool;  // OK.
};
```

## Function Names

Regular functions have mixed case; accessors and mutators may be named like variables.

Ordinarily, functions should start with a lowercase letter and have a capital letter for each new word.

```c++
addTableEntry()
deleteUrl()
openFileOrDie()
```

(The same naming rule applies to class- and namespace-scope constants that are
exposed as part of an API and that are intended to look like functions, because
the fact that they're objects rather than functions is an unimportant
  implementation detail.)

Accessors and mutators (get and set functions) may be named like variables.
These often correspond to actual member variables, but this is not required.
For example, int count() and void set_count(int count).
