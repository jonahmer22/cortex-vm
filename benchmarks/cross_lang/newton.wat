;; Newton's method for sqrt(2.0), 30 iterations.
;; Matches benchmarks/asm/realworld_newton.s.
(module
  (func (export "main") (result f64)
    (local $x f64) (local $n f64) (local $i i32)
    (local.set $n (f64.const 2.0))
    (local.set $x (f64.const 1.0))
    (local.set $i (i32.const 0))
    (loop $continue
      (local.set $x
        (f64.div
          (f64.add (local.get $x) (f64.div (local.get $n) (local.get $x)))
          (f64.const 2.0)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $continue (i32.lt_s (local.get $i) (i32.const 30))))
    (local.get $x)))
