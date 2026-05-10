-- Sieve of Eratosthenes up to 1000. Expected count: 168
-- Matches benchmarks/asm/realworld_sieve.s
local N = 1000
local flag = {}
for i = 0, N - 1 do flag[i] = 1 end
flag[0] = 0
flag[1] = 0
for p = 2, 31 do
    if flag[p] == 1 then
        local j = p * p
        while j < N do
            flag[j] = 0
            j = j + p
        end
    end
end
local count = 0
for i = 0, N - 1 do count = count + flag[i] end
io.write(count)
io.write("\n")
