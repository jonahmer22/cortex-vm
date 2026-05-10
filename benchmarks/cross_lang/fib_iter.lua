-- Iterative Fibonacci, fib(40) = 102334155
-- Matches benchmarks/asm/realworld_fib_iter.s
local a, b = 0, 1
for _ = 1, 39 do
    a, b = b, a + b
end
io.write(b)
io.write("\n")
