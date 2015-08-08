# Notes

This project refers over to a project which accomplished alot with 500 lines of C. This reference project is referred to below as "the model". So it is somewhat self-conscious of its line count.

At present, the line count is about 1300 and finally, object code is coming out yet. Here are some notable excesses:

- Three ways to divide up the code are all included in the source line count including
- - Explicit punctuation (30 lines)
- - Inferred blocks from indentation (layout) (70 lines)
- - Inferred semicolons with open-paren heuristic (100 lines)
- A 100-line comment about expression syntax in Haskell

Obviously this is 200 lines we don't have to have, somewhere in the above.

The model hoovers up the source code into memory before getting going. Then the token scanner attempts to match
the present token and even the present sequence of tokens by giving itself the out that it can bail on an interpretation
and try another one. This is even though the syntax is very much an LL1 syntax. Instead of inferring semicolons freely, an expression continues to the next line if the current line does not end in a semicolon and the next line begins with any whitespace.

Our code uses a conventional tokenizer feeding a conventional recursive-descent syntax analysis. Except of course for the semicolon-inference logic which is at least 70 lines of 'bloat'. A line that does not already end in a semicolon will have a semicolon supplied, unless left parens are unbalanced by right parens. Another criterion that could apply is if an operator is found at the end of the line then don't insert the semicolon, but that doesn't apply to the syntax. The syntax is strictly function application with function names allowed to be strings of the characters that normally are strung together as operators. It would be unlikely to have such a function symbol at the end of an expression, but quite legal.

Both projects compute an AST and then walk it as needed. The model stores information in generic s-lists that are conformant to s-expressions. Our code has a Node type which is a sum type implemented as base and derived classes. There may be a line or two here and there paid on our side for this, which could pay back later when the AST gets more intricate to support datatypes.

The model supports integers and strings, but they are turned into 'lower level' objects, i.e. Scott encodings, by the back-end. The string is of course a list of characters, but those lists are Scott-encoded and the characters are integers and the integers themselves are Scott-encoded which is very close to Church-encoding in space and time. Our code currently supports integers and doubles in the front end, doubles only in the AST, and the numeric type will be 'normal' instead of encoded as function applications.

DCC supports integers, doubles and strings in the syntax. However integers are treated as doubles by the backend. Strings
are treated as lists of integers in the backend.

The model does not support any form of identifier except alphanumeric ones. Our system supports alphanumeric ids with ConIds distinct from varIds, and it supports strings of nonalpha, nonnumeric characters which it tracks as operators. ConIds cannot be function names at present, while operators certainly are treated as function names.
 
The model does not support datatypes in its front end or backend. However it supports cons/car/cdr in its prelude via Scott encoding, along with a maybe type with just and none constructors and a bind function, and booleans and various functional forms.

DCC supports the keyword data as if there were polymorphism, but it isn't checking anything. The keyword heads the definition
of a type name, which is a ConId, which can be defined as the sum of some constructors named with ConIds. Constructors can
have any number of parameters. Thus the data declaration is at the top level, a sum of products. When a constructor is invoked, a structure is created containing the constructor's ID in the header and computations for each constructor argument.

DCC supports the keyword case to start a case expression. Currently branches of a case expression must match on ConIds. If necessary, the backend will force the completion of a computation in order to obtain the ConId. However, that will not force
the constructor arguments. 

DCC currently has built-in operators for arithmetic and comparisons. These operators force their arguments in order to return their own results. 

DCC's conditional construct for "if boolean then this else that" is a supercombinator. The built-in comparison operators return the boolean type defined in the DCC prelude and the if function case-matches the boolean's constructors.

DCC does not have a built-in list except for the implementation of a string. Lists may be constructed using Cons and Nil constructors defined in the prelude, and of course case expressions may deconstruct such lists.

The model does not support a let expression form. The model's prelude and self-hosted compiler exhibit many places where local definitions have been lifted manually. Our code will eventually support either let or where or both. The code to properly deal with recursive lets can be complex.

The model supports monadic I/O using a hack where a dummy argument is wanted by the I/O operators. These operators therefore don't compute until the dummy is passed to the partial application. It remains to be seen how this will work out in our code.

One of the key ideas in the model is that certain niceties from say, Haskell don't repay their cost in a bootstrapping setting. For this project, I want to learn economy of implementation from that example, but ultimately I don't want to strip away everything helpful. An iteration of this compiler with standard infix expressions using operator precedence, for example, would seem reasonable using a precedence-climbing expression parser.

DCC's backend generates C functions. These functions take a pointer to a result on the heap and a pointer to an array of arguments. They create an arbitrary number of local variables on the heap which link together to form a graph of computations to be forced. Eventually something in this structure wants to force a computation, and the graph is maintained in such a way that this result is shared with all consumers of the computation. The backend code requires a C-coded runtime
system that handles this.

At this writing, DCC is in a transition to having unboxed native C types alongside the boxed types. For now any variable's computation or value is stored in a computation. The transition will probably start with unboxed numeric types for intermediate results of arithmetic, which will only live in 'automatic' variables local to a supercombinator. At some point I will start experimenting with a c-call operator designed to support monadicly controlled side-effects.
 