;; Sieve of Eratosthenes up to 1000. Expected count: 168.
;; Matches benchmarks/asm/realworld_sieve.s.
;; flag[i] is stored as a single byte at linear-memory address i.
(module
  (memory 1)
  (func (export "main") (result i32)
    (local $i i32) (local $p i32) (local $j i32) (local $count i32)

    ;; init flag[0..999] = 1
    (local.set $i (i32.const 0))
    (loop $init
      (i32.store8 (local.get $i) (i32.const 1))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $init (i32.lt_s (local.get $i) (i32.const 1000))))
    (i32.store8 (i32.const 0) (i32.const 0))
    (i32.store8 (i32.const 1) (i32.const 0))

    ;; sieve
    (local.set $p (i32.const 2))
    (block $exit_outer
      (loop $outer
        (br_if $exit_outer (i32.ge_s (local.get $p) (i32.const 32)))
        (if (i32.eq (i32.load8_u (local.get $p)) (i32.const 1))
          (then
            (local.set $j (i32.mul (local.get $p) (local.get $p)))
            (block $exit_inner
              (loop $inner
                (br_if $exit_inner (i32.ge_s (local.get $j) (i32.const 1000)))
                (i32.store8 (local.get $j) (i32.const 0))
                (local.set $j (i32.add (local.get $j) (local.get $p)))
                (br $inner)))))
        (local.set $p (i32.add (local.get $p) (i32.const 1)))
        (br $outer)))

    ;; count
    (local.set $count (i32.const 0))
    (local.set $i (i32.const 0))
    (loop $cnt
      (local.set $count (i32.add (local.get $count) (i32.load8_u (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $cnt (i32.lt_s (local.get $i) (i32.const 1000))))
    (local.get $count)))
