

FunC is a bytecode script language interpreter based on Bob Nystrom's "clox" language
as implemented in his work-in-progress book at craftinginterpreters.com


At this time I have completed chapter 28 and I am waiting for the publication of
chapter 29 "Superclasses"
While I am aware that it is possible to complete the original implementation
of clox by cloning and examining his implementation, this is a learning process
for me so I want to read his code explanation rather than just blindly copy his code.

In addition to several of the "challenges" presented in that book,
I have added a lot of other language features. These are marked *EXPERIMENTAL* below.

My primary motivation for implementing this interpreter is a special use case
requiring the scripts to run only a few milliseconds at a time.
This required splitting the interpreter calls into distinct interpret() and run() phases
as well as a few less obvious changes.

I also need multiple interpreters running in paralell, which means I have had to make
significant architectural changes. Most importantly, I have changed the global "vm" struct
to a dynamically allocated one and weaved this into almost every single call.
The book indeed explains that this was omitted for the purpose of keeping the book readable.

Notice that I did *not* need multiple compilers running in paralell and so I have
not done this all the way through the parser/compiler yet.


*********************************************************
HOWEVER! This code comes with a HUGE WARNING!
I am a self-taught hobbyist programmer still learning C
and the code quality clearly reflects this fact.
Using this for anything else than personal fun & learning
would be completely insane! YOU HAVE BEEN WARNED!
*********************************************************



Features:
- arithmetic operators (+, -, *, /)
- modulo operator (%)
- built-in support for null, true and false
- boolean operators (and, or, !)
- lexical scopes
- closures
- c-style if..else statements
- c-style for(;;) loops with break/continue
- c-style while() loops with break/continue
- c-style switch..case statements with break and fall-through (*EXPERIMENTAL*)
- all numbers stored internally using 64bit "double"
- all strings stored internally using zero terminated strings
- automatic string internalization and deduplication
- automatic memory management with garbage collection
- classes and objects
- methods and initializers
- inc/dec operators (++, --, +=, -=, *=, /=) (*EXPERIMENTAL*)
- bitwise operators (<<, >>, ~, &, |, ^) (*EXPERIMENTAL*)
- ternary conditional expressions (?:) (*EXPERIMENTAL*)
- arrays can be initialized, indexed and assigned to with [] (*EXPERIMENTAL*)
- arrays can be sliced and spliced with [:] (*EXPERIMENTAL*)


ARRAY values support the following *EXPERIMENTAL* built-in attributes:
  [].length                return length of array as a number value
  [].push(v1, v2, vN)      append value(s) to array, return new array
  [v1, v2, vN].pop()       remove last value from array, return value
  [].unshift(v1, v2, vN)   prepend value(s) to array, return new array
  [v1, v2, vN].shift()     remove first value from array, return value
  [].resize(N)             extend or truncate table to specified size
  [].fill(V)               set all elements in array to specified value
  [].join(S)               return String with all elements joined using String delimiter
  [].flat()                return a new array with all elements from nested array

NUMBER values support the following *EXPERIMENTAL* built-in attributes:
  0.base(a)        return string representation in base 2..36 inclusive
  0.str            shorthand for .base(10); return number as string
  0.char           return utf8 character as string

  0.atan2(x)       y.atan2(x) returns the arctangent of y, x
  0.pow(y)         x.pow(y) returns x to the power of y
  0.fmod(y)        x.fmod(y) returns the remainder of x divided by y
  0.hypot(y)       x.hypot(y) returns the hypotenuse of x,y = sqrt(x*x + y*y)

  // The following are thin wrappers around the corresponding C math functions
  0.floor          return floor value (e.g. 1.2 => 1.0)
  0.ceil           return ceiling value (e.g. 1.2 => 2.0)
  0.abs            return absolute value
  0.sqrt           return square root
  0.sin
  0.cos
  0.tan
  0.asin
  0.acos
  0.atan
  0.sinh
  0.cosh
  0.tanh
  0.asinh
  0.acosh
  0.atanh
  0.exp
  0.log
  0.log10
  0.cbrt

STRING values support the following *EXPERIMENTAL* built-in attributes:
  "".bytes          return number of bytes in string
  "".chars          return number of utf8 code points in string
  "".byte_at(a)     return byte at offset a
  "".char_at(a)     return utf8 code point number a
  "".bytes_at(a,b)  return b bytes starting at offset a (negative values = count from end)
  "".substr(a,b)    return b codepoints starting at number a (negative values = count from end)
  "".value(a)       return number value of string in base 2..36 inclusive (default=10 if not specified)
  "".num            shorthand for .value(10); return numeric value in base 10
  "".split(a)       return array containing string parts using a as delimiter (individual BYTES if "")

