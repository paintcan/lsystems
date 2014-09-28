; the axiom is the starting string for the l-system
(axiom (B 2) (A 4 4))

; the production rule is of form LEFT < CENTER > RIGHT
; a production is only matched if the expression
; for the production evaluates to true on the params
; collected for all the symbols to left, center,
; and right. left and right rules are optional, but if
; more than one symbol is specified it is assumed that
; the current rule is a LEFT rule, because CENTER may
; only consist of one symbol.

; note that contained trees are ignored for pattern matching,
; so the symbols being matched may have any number of children
; trees in between them!

; (production (left < center > right) expression result)

; expression is expected to be an s-expression - () may be used
; if there is no conditional evaluation for this production
; result is expected to be a list of s-expressions

(production ((A x y))
	    (<= y 3)
	    (A (* x 2) (+ x y)))

(production ((A x y))
	    (> y 3)
	    (B x) (A (/ x y) 0))

(production ((B x))
	    (< x 1)
	    (C))

(production ((B x))
	    (>= x 1)
	    (B (- x 1)))

; this example l-system is taken from chapter 1 of "The Algorithmic
; Beauty of Plants"