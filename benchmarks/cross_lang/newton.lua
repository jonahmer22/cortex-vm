-- Newton's method for sqrt(2.0), 1,000,000 iterations.
-- Converges in ~50 iterations; the remainder stress-tests the float loop.
-- Matches benchmarks/asm/realworld_newton.s
local n = 2.0
local x = 1.0
for _ = 1, 1000000 do
    x = (x + n / x) / 2.0
end
io.write(string.format("%.6f\n", x))
