[Source Reddit comment: https://www.reddit.com/r/brainfuck/comments/f25ddg/x/fhbv2a9
 URL in the comment: http://www.hevanet.com/cristofd/brainfuck/fib_explained.b]

[This program computes Fibonacci numbers. We don't assume either that cells
are bytes or that they're not bytes, because implementations vary greatly on
this point. Since the Fibonacci numbers grow too large to fit in a cell, we'll
spread them across many cells. (Even if correctness did not require this, it
would be very good for speed.) We want to output the numbers in decimal, so it
seems easiest to store one decimal digit per cell.

We keep the last two numbers computed (call them A and B); we output B, then
replace A and B with B and A+B, respectively. We can do the addition and
replacement one digit at a time, so there's never a need to store all three
numbers (A, B, and A+B) in their entirety.

Because the array is unbounded on the right, we store the numbers with most
significant digit to the right so they can grow that way. So we output B from
right to left, and update A and B from left to right, carrying 1s as needed.

It proves most convenient to keep A and B interspersed with each other: it puts
corresponding digits of both near each other, and lets them both grow without
limit without having to move either number around. A third set of cells (call
them T) will be used as temporary variables, and at other times kept nonzero to
mark the length of B (the longer number; it's not worth keeping track of the
length of A separately, as it's either the same length or one digit shorter).

The memory layout is then:
0 10 A T B A T B ... A T B 0 0 0 ...

I should note that fib.b was the first nontrivial brainfuck program I wrote, in
2001 I think, and this version is very close to that one. The part I found
hardest was how to carry the 1 when the sum at a particular digit was 10 or
more. Eventually, I figured out that I could use basically a case statement,
decrementing the sum in a set of nested loops that peel off one case at a time.
This is not the most concise solution possible, though it may be the quickest.]

>++++++++++>+>+                      Initialize linefeed=10 A=1 T=1 B=0
[                                    Main loop: each iteration outputs one
                                         Fibonacci number and computes next one
    [                                Output loop: output one digit per iteration
        +++++[>++++++++<-]           Increase B by 48 for visible output
                                     (ASCII codes for the printable characters
                                         '0' through '9' are 48 through 57)
        >.<                          Output this digit of B
        ++++++[>--------<-]          Decrease B by 48 to restore it
        +<<<                         Restore T=1 and go left to next digit of B
    ]                                Output loop ended; all digits of B output
    >.>>                             Output linefeed and go back to leftmost T
    [                                Update loop: each iteration updates one
                                         digit of A and B (setting A to B and
                                         B to sum of A and B with carries)
        [-]                          Clear T to 0 (was either 1 or 2 before)
        <[>+<-]                      Move this digit of A to T for the moment
        >>[<<+>+>-]                  Set A=B and T=A plus B (may exceed 9 now;
                                         so we may need to carry a 1 next)
        <[>+<-[>+<-[>+<-[>+<-[>+<-[  Start moving sum from T to B gradually
                                         (if sum was 0 to 5 we skip forward as
                                         soon as we finish moving it)
            >+<-[>+<-[>+<-[>+<-      Continue (if sum was greater than 5)
            [                        Sum exceeded 9 so we need to carry a 1
                >[-]                 Set B=0 (last digit of "10")
                >+                   Add 1 to next digit of A (a way to carry 1
                                         into the next digit of the sum because
                                         in the next iteration of update loop
                                         that digit of A will go into the sum
                                         and will not persist apart from that)
                >+                   Add 1 to next T to make sure it's nonzero
                                         (in case the sum is longer than old B
                                         and this is last carry lengthening B)
                <<<-                 Decrease the sum by the 1 we just added
                                         (still moving sum to B gradually; this
                                         long case just changed "9" into "10")
                [>+<-]               Move rest of sum to B (if sum was between
                                         11 and 19 then B=1 to 9 to match)
            ]]]]]]]]]]               Ends of skip loops for cases 0 through 9
        +>>>                         Restore T=1 and go on to update next digit
    ]                                Loop done: all digits have been updated
    <<<                              Go back to rightmost T ready to output sum
]                                    End of main loop which never terminates

[This program showed me:
-That handling variable-sized data is necessary and entirely feasible
-Breadcrumb technique for scanning through data with [>>>] (optionally
doing more things at each cell as in this case)
-Interspersed storage technique for giving several numbers their own space
-Don't keep data longer than you need it (heuristic)
-Keep written maps of the memory layout at different times; brainfuck is less
self-documenting than most languages, so your code alone isn't enough
(In this case the map might look something like this during the value-update part:
0  10  a  t' b  a  t  b  0  0  0
0  10  a' 0  b  a  t  b  0  0  0
0  10  0  a  b' a  t  b  0  0  0
0  10  b  c' 0  a  t  b  0  0  0
0  10  b  0' c  a  t  b  0  0  0
0  10  b  t  c  a  t' b  0  0  0
It works fine to keep these with pen and paper too.)
-Case statement technique (next used for a caricaturish ROT13 program)
-To consider reversing reversible transformations rather than making temporary
copies of values. x+48-48=x.
-That a brainfuck program needn't be terribly slow or complicated. (This one
executes <450 commands per character of output.)]
