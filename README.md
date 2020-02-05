

FunC is a bytecode script language interpreter based on Bob Nystrom's "clox" language
as implemented in his work-in-progress book at craftinginterpreters.com


At this time I have completed chapter 27 and I am waiting for the publication of
chapter 28 "Methods and Initializers"
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

Notice that I did *not* need multiple compilers running in paralell and so I have limited
this feature to the runtime parts as of yet.


*********************************************************
HOWEVER! This code comes with a HUGE WARNING!
I am a self-taught hobbyist programmer still learning C
and the code quality clearly reflects this fact.
Using this for anything else than personal fun & learning
would be completely insane! YOU HAVE BEEN WARNED!
*********************************************************



Features:
- arithmetic operators (+, -, *, /)
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
- inc/dec operators (++, --, +=, -=, *=, /=) (*EXPERIMENTAL*)
- bitwise operators (<<, >>, ~, &, |, ^) (*EXPERIMENTAL*)
- ternary conditional expressions (?:) (*EXPERIMENTAL*)
- arrays can be initialized, indexed and assigned to with [] (*EXPERIMENTAL*)
- arrays can be sliced and spliced with [:] (*EXPERIMENTAL*)



