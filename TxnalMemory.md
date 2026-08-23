# Design of Virgil with Transactional Memory

In part to support ongoing research in some research group(s) and in part to
explore the possibility of transactional memory as an approach to managing
concurrency in Virgil, we are undertaking the design and implementation of
transactional memory for Virgil.  Because the words /transaction/ and
/transactional/ are kind of long, we will tend to use /atomic/ and its antonym
/nonatomic/ instead.

Our goal is fairly minimal extension of Virgil syntax and semantics and
ability to target Web Assembly's Simple Transactions extension (hereafter
STE).

## Types

It is helpful first to categorize the existing types in Virgil:

* Value types: Types whose elements are pure values, and thus inhernetly not
  subject to transactions' concurrency control or rollback semantics.  These
  include:
  - Variants (ADTs)
  - Tuples
  - Functions
  - Numerics (integer and floating point types)
  - Enums
  - Enum sets
  - Pointers
* Mutable types: Types whose elements can be mutable
  - Classes
  - Arrays
  - Ranges
  - Refs
  - Components (not types exactly, but possibly containing mutable data)

Our design adds /atomic/ versions of mutable types, which obey transaction
semantics (as detailed below).  They are distinguished from their /nonatomic/
versions in that they have transactional metadata associated with them to
support tranactional semantics.  The default version of a mutable type is its
atomic one.  We add a new built-in type constructor `Nonatomic<T>` to obtain
the nonatomic version of `T`.  If `T` is already nonatomic, or is a value
type, then `T` and `Nonatomic<T>` are the same type.

In sum, there are three kinds of types, mutable atomic types, mutable
nonatomic types, and value types.  Sometimes it will be useful to know that
references to mutable types are /values/; it is the /data/ they refer to that
is mutable.

// We do not specify the granularity of concurrency control and rollback, though
// a natural implementation would add per-object metadata to instances of atomic
// types.

## Code

Our design distinguishes three kinds of code: /atomic/, /nonatomic/, and
/relaxed/.  To enforce transaction semantics, some actions are allowed only in
atomic code and some only in nonatomic code.  All actions are allowed in
relaxed code.

Each method (including `new`) has a code kind that determines the kind of the
code within it, though it may contain nested scopes of other kinds.  To
override the default code kind of a given method, or just make the code kind
explicit, prefix its definition with the appropriate keyword, `atomic`,
`nonatomic`, or `relaxed`.

Constructs that can contain method definitions, both mutable (classes,
components) and value (variants, enums) may likewise be prefixed with
`atomic`, `nonatomic`, or `relaxed`, which indicates the default code kind for
the methods within it.  The default code kind for all constructs is `atomic`,
with the exception that `main` methods are `nonatomic` by default.

The design also defines `atomic` and `relaxed` code blocks.
An `atomic` block has the form
```
atomic { ... statements ... }
```
or
```
atomic { ... statements ... } except (v: T) { ... except-body ... }
```
The contained `statements` are atomic code.  The `except-body` is useful only
if the immediately surrounding code is nonatomic, and it is itself nonatomic.

A `relaxed` block has the form
```
relaxed { ... statements ... }
```
The contained `statements` are relaxed code.

### Code nested and calling rules

An `atomic` block is allowed in any code.  It is useful only if its
immediately surrounding code is not atomic.  A `relaxed` block is also allowed
in any code.  It is useful only if its immediately sorrounding code is not
relaxed.

Any code may call atomic methods.  Only atomic and relaxed code may call
relaxed methods.  Only nonatomic and relaxed code may call nonatomic methods.

## Transactions Semantics

### Transaction Boundaries

As with ordinary Virgil, execution normally flows from block to block, control
by `if`, `for`, `while`, etc., statements, and calls and returns.  At any
given time, execution is within a transaction or it is not.  Initially, it is
not.  When not already in a transaction, a transition from nonatomic to atomic
or relaxed code /starts/ a transaction.  The matching transaction out of
atomic or relaxed code, which happens by exiting the block or method that
started the transaction, /ends/ the transaction.

A transaction may /succeed/ (also called /commit/) or /fail/.  If the code
that started the transaction is an `atomic` block with an `except` clause, a
failure results in transition to the `except-body` with a value provided by
the run-time system.  The form `except (v: T)` decalres variable `v`, of type
`T`, to hold this value.  The scope of `v` is the `except-body`.  The initial
implementation limits the type `T` to be `int`.  If execution reaches the end
of `except-body`, it continues with the code following the `atomic` block.

If a transaction fails, and it was not started by an `atomic` block with an
`except` clause, the run-time system will retry the transaction, making a
best-effort attempt to complete it successfully, if possible.

### Accesses and Conflicts

To define the semantics of transaction more completely, we describe /accesses/
to mutable data.  When code examines a mutable field of an instance of a
mutable type, we call that a /read access/, or simply a /read/.  When code
assigns to or updates a mutable field of an instance of mutable type, we call
that a /write access/, or simply a /write/.  Two accesses /conflict/ if they
are made by two different concurrently tranactions to the same data item of an
atomic type and at least one of the accesses is a /write/.  Initialization of
a `def` field of an instance of mutable type, with a value that is not a
compile-time known constant, is also considered to be a write.  When a
conflict occurs, at least one of the two transactions involved must fail.

### Rollback

If a transaction fails, all mutations it has made to instances of atomic
mutable types, except to instances it created, must be discarded.

### Visibility

While a transaction runs, no other transaction should be able to see any of
its mutations to instances of atomic types.  If the transaction succeeds, then
these mutations will become visible.

### Relation to ACID Transaction Semantics

When no relaxed code is executed, the semantics of Virgil transactions meet
the ACI (atomicity, consistency, and isolation) properties, but not the D
(durability) property, of standard transaction semantics.  However, we
explicitly permit implementations that use optimistic reads with later
validation, which technically violates isolation (but only for transactions
that fail).

Relaxed code can violate ACID properties and must be employed with care.  A
key use for it that we envision is building I/O libraries, which ideally will
support some form of transactional I/O streams / queues / buffers.

### Accesses and Code Kinds

Unless specified otherwise, accesses to instances of atomic types must occur
in atomic or relaxed code, and accesses to instance of nonatomic types must
occur in nonatomic or relaxed code.  As a possible convenience to the
programmer, we permit accesses to atomic types in /nonatomic/ code, by
implicitly making them short atomic blocks.  Generally there will be one block
per access, but the compiler translate a construct such as `counter++`, which
may have more than one access (a read and write) into a single block.

## Granularity

The implementation ultimately defines the granularity of conflict detection
and rollback.  Our intended first target is STE, which uses object granularity
for heap instances (classes, arrays) and field granularity for global
variables (fields of components).

## SSA

One of the overheads of transactions is the support for conflict detection and
rollback.  To help reduce that cost, STE adopted the notion of references to
atomic objects where the references have additional /permissions/.  A
reference with /read/ permission allows read accesses to its referent's data
and one with /write/ permission allows read and write accesses to its
referent's data.  We will mirror that approach by adding `AcquireRead` and
`AcquireWrite` SSA instructions that take as their argument a reference to an
atomic instance, with possibly weaker permissions on the reference, and return
a reference that has been checked to allow the indicated kinds of access
within the current transaction.

We note that while these instructions semantically copy their argument, an
actual run-time system's implementation may use different values for a
reference without permissions and one that has permissions.  For example, in a
system that uses an object table, a no-permissions reference may be an object
id, while one /with/ permissions may point directly to the object data.  Since
acquiring write access may result in making a new copy of the object, an
`AcquireWrite` should drop any `AcquireRead` on the same object (since the
older `AcquireRead` may point to the older version of the object).

When exiting a transaction, all `AcquireRead` and `AcquireWrite` SSA variables
should either be dropped or demoted to a no-permission form using a new SSA
instruction, `DropPermissions`.  This insures that permissions are reacquired
in any new transaction.

Some useful properties of thes instructions include:

- `x` and `AcquireRead x` refer to the same object
- `x` and `AcquireWrite x` refer to the same object
- `AcquireRead x` and `AcquireWrite x` refer to the same object
- `x` and `DropPermissions x` refer to the same object
- `AcquireRead (AcquireRead x) = AcquireRead x`
- `AcquireRead (AcquireWrite x) = AcquireWrite x`
- `AcquireWrite (AcquireRead x) = AcquireWrite x`
- `AcquireWrite (AcquireWrite x) = AcquireWrite x`
- If `x` has no permissions, then `DropPermissions x = x`
- `DropPermissions (AcquireRead x) = DropPermissions x`
- `DropPermissions (AcquireWrite x) = DropPermissions x`
- If `x` and `y` refer to the same object, then
   * so do `AcquireRead x` and `AcquireRead y`
   * so do `AcquireWrite x` and `AcquireWrite y`
   * so do `DropPermissions x` and `DropPermissions y`
- If `x` and `y` refer to different objects, then
   * so do `AcquireRead x` and `AcquireRead y`
   * so do `AcquireWrite x` and `AcquireWrite y`
   * so do `DropPermissions x` and `DropPermissions y`

We will also need SSA instructions for:

* Transaction start: `TxnBegin`
* Transaction end: `TxnEnd`

And also some way to deal with `except` clauses.  Perhaps this could work:

* Transaction start with except clause: `TxnBeginExcept`, which has two
  control flow successors, the normal block and the except block, somewhat
  like an `if` (but without an expression to test, since the except control
  transfer is generated "magically" by the run-time system).

## Extending Permissions

We could allow annotations of atomic arguments to atomic and relaxed methods
to allow acquired permissions to flow into calls.  We could (optionally)
prefix argument types in method headers (and function types) with `@read` or
`@write` annotations, indicating the those permissions shiould already be
acquired.  At a call site, the compiler would emit SSA to insure those
permissions are acquired before the call.

## Conditional Critical Regions, and Retry

Following Harris and Fraser, we can extend `atomic` blocks to include a
condition:
```
atomic (expr) { ... statements ... } [ except (v: T) { ... except-body ... } ]
```
This evaluates `expr`, which must be of type `bool`, within the transaction.
If `expr` evaluates to `true`, execution continues, but if it evaluates to
`false`, the transaction will be retried.  Preferably, in that case, the
compiler and run-time systen cause execution to wait until at least one atomic
data item accessed by the transaction is changed (by some other transaction).
The waiting transaction then fails and retries.  The waiting can be
accomplished by having the transaction continue to hold its resources, but in
a manner where current or future conflict is detected and the transaction then
fails in favor of the conflicting transaction.

For fkexibility beyond a single predicate test at the beginning of a
transaction, we can add an explicit wait call:
```
Transactions.wait(); // wait for any object accessed by this transaction to change
```
Also, as mentioned in Harris and Fraser, the compiler and run-time may need to
guarantee that loops in transactions occasionally do a transaction
/validation/ if the run-time can violate isolation for transactions that will
eventually fail.  The low-level primitive for that can be exposed if we like:
```
Transactions.validate();  // validate the current transaction
```
We can further offer a primitive supporting busy-waiting with a chosen delay
before the next retry:
```
Transactions.retry(n: long);  // retry after n nanoseconds
```

## Thread-Local Variables

It would be interesting to think through the possibility of treating
thread-local variables specially within transactions.  Atomic thread-local
variables would need rollback support but not concurrency control, simplifying
their implementation.  When used with a relaxed block, nonatomic thread-local
variables can provide a safe way for a failing transaction to return results
detailing the failure, among other capabilities.

## For Future Consideration

Come up with a way for parameterized types to deal with whether their type
parameters are atomic, nonatomic, or value types - to treat them differently
and use them to develop atomic / nonatomic annotations for types used inside
the parameterized construct.

## Thoughts on implementation strategy

Basic strategy: front end to back end

- Add a FEATURE test
- Add a command line flag, -tmem (for "transactional memory")
- Determine what AST needs to be added, and any fields in current AST that
  need work
- Extend the parser
- Determine what additional checking is required
- Extend the validator
- Determine details of new SSA instructions, and any modifications of existing
  ones
   * Add the new instruction
   * Deal with impacts / adjustments to normalization, optimization
   * Deal with impacts / adjustments to lowering
   * Develop Wasm Transactions (STE) back-end as a delta on the Wasm GC one
- Any run-time system changes needed
