# Design Log - Project 2

## Growth factor and amortized cost

For the growable array, I went with a capacity doubling strategy starting from 1, then 2, 4, 8, 16, and so on. Reallocation only triggers when the current size hits the maximum capacity. To prove why this is efficient, we have to look at the total copying work done over n appends. Every time the array doubles, it copies the existing elements. The number of copies for each reallocation forms a geometric series: 1 + 2 + 4 + 8 + ... + n/2. The sum of this geometric series is less than 2n, so the total copying work across n appends is O(n). Dividing that total work across the n appends gives an O(1) amortized cost per append. This means that while individual resizes might be slow, the average cost per insertion remains constant, so the array doesn't get bogged down as it grows.

## Rule of Five evidence

To manage memory safely, I implemented the Rule of Five:

- **Destructor:** Cleans up allocated memory by calling `delete[]` on the `data_` array to prevent memory leaks.
- **Copy Constructor:** Allocates its own independent storage so the new object gets a full copy of the data without causing shallow copy bugs.
- **Copy Assignment:** Checks for self-assignment first, but instead of copying directly into the existing object, it first allocates a completely new array and copies the source data into it. Only after that succeeds does it delete the old `data_` array and replace the pointer. This distinction is important because if the allocation fails and throws an exception before `delete[]` is called, the original object remains perfectly intact.
- **Move Constructor:** Steals the pointer directly from the source object and empties the source by setting `data_ = nullptr`, `size_ = 0`, and `capacity_ = 0`.
- **Move Assignment:** First releases any existing storage the current object is holding. Then, it steals the source's pointer and resets the source's `data_`, `size_`, and `capacity_` fields to zero or null. This directly matches the project requirement that moved-from objects must remain completely valid and empty.

## Sentinel scanner: bounded pending_ proof

The memory for the `SentinelScanner` stays bounded because the `pending_` buffer only holds onto characters that could potentially be the start of the sentinel string. Specifically, the implementation finds the longest suffix of the current text that is also a prefix of the sentinel. To prove the strict memory bound: `pending_` only stores this prefix candidate. If that candidate ever reached a length exactly equal to `sentinel_.size()`, then the full sentinel would have been found, detected, and cleared. Therefore, any unresolved candidate stored in the buffer must have a length of at most `sentinel_.size() - 1`. Because any older characters that do not form part of this valid prefix candidate can no longer contribute to a future sentinel match, they are safely emitted to the output immediately. It never actually has to store the entire reply in memory at once, guaranteeing the memory footprint stays strictly capped.

## What I would change differently

If I were to refactor this project, I would definitely reconsider how I wrote the copy assignment operator and use the copy-and-swap idiom instead. Writing out the self-assignment check and manual deep copy works fine, but copy-and-swap is cleaner and much safer. With the required `const Conversation&` interface, I could still use a copy-and-swap-style implementation by first creating a temporary copy of the other object, swapping its owned storage with the current object, and allowing the temporary to destroy the old storage when it goes out of scope. Because the copy happens first, if it throws an exception, the original destination object remains completely unchanged. This approach would have cut out some redundant code and made the class simpler to reason about and more strongly exception-safe.
