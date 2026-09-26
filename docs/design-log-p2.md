# Design Log - Project 2

## Growth factor and amortized cost

For the dynamic array, I doubled the capacity every time it ran out of room starting at 1, then going 2, 4, 8, 16, and so on. Reallocation only happens when the array hits its current capacity. To show why this is efficient, I looked at how much copying work happens over n appends. Every time the array doubles, it has to copy over all the existing elements. The copies for each resize add up to 1 + 2 + 4 + 8 + ... + n/2, which sums to less than 2n. That means the overall copying work across all n appends is just O(n). Averaging that out over all n appends gives an O(1) amortized cost per append. So even though a resize takes a quick hit, the average time per insertion stays constant, keeping the array fast as it grows.

## Rule of Five evidence

To handle memory safely, I implemented the Rule of Five across the class. My destructor cleans up dynamically allocated memory by calling delete[] on data_ so there are no memory leaks. For the copy constructor, copy assignment, and append reallocation, temporary arrays are owned by std::unique_ptr while messages are copied. That temporary owner releases the array only after every copy succeeds, so a throwing message copy cannot leak the partially filled array. Copy assignment also completes the new copy before deleting the old data_, so the original object remains unchanged if allocation or copying fails. For the move constructor, I steal the pointer directly from the source object and reset its data_ to nullptr while setting size_ and capacity_ to zero. Finally, my move assignment frees whatever storage the current object is already holding, grabs the source's pointer, and zeros out its fields so the moved-from object remains valid and empty.

## Sentinel scanner: bounded pending_ proof

To keep memory bounded in SentinelScanner, I made sure my pending_ buffer only holds onto characters that could actually start a sentinel match. Basically, I track the longest suffix of the text so far that matches a prefix of the sentinel string. To show why the memory bound holds, if the pending_ buffer ever reached sentinel_.size(), the full sentinel would have already been detected and cleared. So, any unresolved candidate in the buffer can only ever reach a max length of sentinel_.size() - 1. Any older characters that can't be part of the match get flushed straight to the output immediately. This means I never have to store the entire response in memory at once, keeping the memory footprint strictly capped.

## What I would change differently

If I were to redo this project, I'd still consider using the copy-and-swap idiom for copy assignment. Writing out the manual self-assignment check and deep copy works, and the temporary unique_ptr now handles cleanup if an element copy throws, but copy and swap would make the commit step more uniform. Even with the required const Conversation& interface, I could make a temporary copy of the source object, swap its storage with the current object, and let the temporary clean up the old memory when it goes out of scope.
