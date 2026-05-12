;; Recursive Fibonacci, fib(35) = 9227465
;; Matches benchmarks/asm/realworld_fib_rec.s
(module
  (func $fib (param $n i64) (result i64)
    (if (result i64) (i64.lt_s (local.get $n) (i64.const 2))
      (then (local.get $n))
      (else
        (i64.add
          (call $fib (i64.sub (local.get $n) (i64.const 1)))
          (call $fib (i64.sub (local.get $n) (i64.const 2)))))))
  (func (export "main") (result i64)
    (call $fib (i64.const 35))))
