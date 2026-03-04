TODO list for adding new feature: extended enum

Description:  Extended enums introduce open subtypes to enums, rather
analogous to the open types extension to variants.  The feature can be added
in these stages:

1. Allow an _ "case" in enum types.
   Example: enum E1 { A, B, _ }
   Example: enum E2 { _ }
   Todos:
   - Extend the syntax
   - Extend the semantic checking
2. Allow subtype enums for enums that have an _ case
   Example: enum E1.More { C, D }
   Example: enum E1.EvenMore { E, F, _ }
   Example: enum E1.EvenMore.Deeper { G }
   We should be able to deal with default values for enum types similarly to default values for variants.
   Subtype enums must include the fields of their supertype.
   The .name for E1.More.C should be "More.C" and for E1.EvenMore.Deeper.G should be "EvenMore.Deeper.C" (subtype enum case names should include their subtype's name)
   Add .shortName which gives only the last component of the name: E1.More.C.shortName should be "C", etc.
   Todos:
   - Extend the syntax
   - Extend the semantic checking
   - Add implmentation for .shortName
3. Allow methods for enum types.
   Example: enum E1 { A { def m1() => 1; }, B, _ { def m2() -> bool; }; def m1() => 0; }
     Note that the enum's methods are separated from its cases by a ';'.
   Overriding should be analogous to open variant types.
   Todos:
   - [x] Extend the syntax (sub-stage 3.1: enum-level methods after ';')
   - [x] Extend the semantic checking (sub-stage 3.1: basic method resolution)
   - [ ] Per-case overrides (sub-stage 3.2)
   - [ ] Subtype method inheritance with overrides (sub-stage 3.3)
   - Implementation may be helped by inventing *boxed enum objects*, for use when forming closures of enum methods.
     - A boxed enum can have a single field, holding the enum's tag value.
     - Method dispatch can be done using the enum tag value.
     - A pre-constructed global array indexed by enum tag can be used to get
       an enum's boxed value, when needed
     - Enum values should usually be unboxed, represented by their enum tag value
     - Enum fields should continue to be held in global arrays indexed by enum tag value
4. Allow subtypes to redeclare supertype fields [DONE]
   Given supertype enum E3(x: int) { A(1), B(17), _ }, example subtype
   declarations:
     enum E3.S1(x: int) { C(10) } // if field declarations are repeated, they must use the same names and types, in the same order, as the supertype
     enum E3.S2(super) { D(15) } // the keyword 'super' means "repeat the supertype fields here":
     enum E3.S3 { H(23) }  // restating supertype fields is not required
   Todos:
   - [x] extend syntax
   - [x] extend semantics
5. Allow subtypes to *add* fields [DONE]
   Given supertype enum E4(b: bool, i: int) { A(true, 0), B(false, 1), _ },
   example subtype declarations that add new fields:
     enum E4.S1(b: bool, i: int, f: float) { F(false, 2, 1.0f) }  // restate supertype field and add new ones
     enum E4.S2(super, f: float) { G(true, 3, 4.5f) } // use 'super' to indicate presence of supertype fields
     enum E4.S3(f: float) { H(true, 2, 5.1f) } // error: must restate supertype fields or use super when adding fields
   Added fields can be implemented using a global array just as original enum fields do
   Multi-level field inheritance works: 'super' means "parent's effective params"
   (root params + all intermediate ancestors' extras). Grandchild E.S.T inherits
   both root E's fields and intermediate E.S's extra fields.
   Todos:
   - [x] extend syntax
   - [x] extend semantics
   - [x] extend implementation
   - [x] multi-level field inheritance
