-- Recursive Fibonacci, fib(30) = 832040
-- Matches benchmarks/asm/realworld_fib_rec.s
local function fib(n)
    if n < 2 then return n end
    return fib(n - 1) + fib(n - 2)
end
io.write(fib(30))
io.write("\n")
