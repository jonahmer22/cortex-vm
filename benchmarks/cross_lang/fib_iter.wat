;; Iterative Fibonacci, fib(40) = 102334155
;; Matches benchmarks/asm/realworld_fib_iter.s
(module
  (func (export "main") (result i64)
    (local $a i64) (local $b i64) (local $tmp i64) (local $i i32)
    (local.set $a (i64.const 0))
    (local.set $b (i64.const 1))
    (local.set $i (i32.const 0))
    (block $exit
      (loop $continue
        (br_if $exit (i32.ge_s (local.get $i) (i32.const 39)))
        (local.set $tmp (i64.add (local.get $a) (local.get $b)))
        (local.set $a (local.get $b))
        (local.set $b (local.get $tmp))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $continue)))
    (local.get $b)))
