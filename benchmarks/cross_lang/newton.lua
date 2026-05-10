-- Newton's method for sqrt(2.0), 30 iterations.
-- Matches benchmarks/asm/realworld_newton.s
local n = 2.0
local x = 1.0
for _ = 1, 30 do
    x = (x + n / x) / 2.0
end
io.write(string.format("%.6f\n", x))
