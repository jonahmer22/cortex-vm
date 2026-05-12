-- Iterative Fibonacci, fib(40) = 102334155
-- Runs 1,000,000 outer iterations to amortize startup cost.
-- Matches benchmarks/asm/realworld_fib_iter.s
local a, b, tmp
for _ = 1, 1000000 do
    a, b = 0, 1
    for _ = 1, 39 do
        tmp = a + b
        a = b
        b = tmp
    end
end
io.write(b)
io.write("\n")
