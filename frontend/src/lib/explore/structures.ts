// Small data structures behind the Compare + Saved screens.
// Kept free of Svelte and the DOM so they can be unit tested directly.

export type Comparator<T> = (a: T, b: T) => number;

/**
 * Stable merge sort. O(n log n), returns a new array and never changes the input.
 * "Stable" means two equal items keep their original order.
 */
export function mergeSort<T>(items: readonly T[], compare: Comparator<T>): T[] {
	if (items.length <= 1) return [...items];

	const middle = Math.floor(items.length / 2);
	return merge(
		mergeSort(items.slice(0, middle), compare),
		mergeSort(items.slice(middle), compare),
		compare
	);
}

function merge<T>(left: readonly T[], right: readonly T[], compare: Comparator<T>): T[] {
	const merged: T[] = [];
	let l = 0;
	let r = 0;

	while (l < left.length && r < right.length) {
		// "<=" keeps equal items in their original order.
		if (compare(left[l], right[r]) <= 0) merged.push(left[l++]);
		else merged.push(right[r++]);
	}
	while (l < left.length) merged.push(left[l++]);
	while (r < right.length) merged.push(right[r++]);
	return merged;
}

/** Binary heap. `comesFirst` returns a negative number when `a` belongs above `b`. */
class BinaryHeap<T> {
	private items: T[] = [];

	constructor(private readonly comesFirst: Comparator<T>) {}

	get size(): number {
		return this.items.length;
	}

	peek(): T | undefined {
		return this.items[0];
	}

	push(value: T): void {
		this.items.push(value);
		this.bubbleUp(this.items.length - 1);
	}

	pop(): T | undefined {
		if (this.items.length === 0) return undefined;
		const top = this.items[0];
		const last = this.items.pop() as T;
		if (this.items.length > 0) {
			this.items[0] = last;
			this.bubbleDown(0);
		}
		return top;
	}

	private bubbleUp(index: number): void {
		let child = index;
		while (child > 0) {
			const parent = Math.floor((child - 1) / 2);
			if (this.comesFirst(this.items[child], this.items[parent]) >= 0) break;
			[this.items[child], this.items[parent]] = [this.items[parent], this.items[child]];
			child = parent;
		}
	}

	private bubbleDown(index: number): void {
		let parent = index;
		for (;;) {
			const left = parent * 2 + 1;
			const right = left + 1;
			let best = parent;
			if (left < this.items.length && this.comesFirst(this.items[left], this.items[best]) < 0) {
				best = left;
			}
			if (right < this.items.length && this.comesFirst(this.items[right], this.items[best]) < 0) {
				best = right;
			}
			if (best === parent) break;
			[this.items[parent], this.items[best]] = [this.items[best], this.items[parent]];
			parent = best;
		}
	}
}

/** Smallest item comes out first. Used for the nearest deadline. */
export class MinHeap<T> extends BinaryHeap<T> {
	constructor(compare: Comparator<T>) {
		super(compare);
	}
}

/** Largest item comes out first. Used for the top recommendations. */
export class MaxHeap<T> extends BinaryHeap<T> {
	constructor(compare: Comparator<T>) {
		super((a, b) => -compare(a, b));
	}
}

/**
 * The k highest-scoring items, best first.
 * Ties keep the earlier item first, so recommendations do not jump around.
 */
export function topK<T>(items: readonly T[], score: (item: T) => number, k: number): T[] {
	if (k <= 0) return [];

	type Scored = { item: T; score: number; index: number };
	const heap = new MaxHeap<Scored>((a, b) => a.score - b.score || b.index - a.index);
	for (let index = 0; index < items.length; index++) {
		heap.push({ item: items[index], score: score(items[index]), index });
	}

	const result: T[] = [];
	while (result.length < k && heap.size > 0) {
		result.push((heap.pop() as Scored).item);
	}
	return result;
}

/** First-in, first-out queue. Notifications are read oldest first. */
export class NotificationQueue<T> {
	private items: T[] = [];

	static from<T>(items: readonly T[]): NotificationQueue<T> {
		const queue = new NotificationQueue<T>();
		for (const item of items) queue.enqueue(item);
		return queue;
	}

	enqueue(item: T): void {
		this.items.push(item);
	}

	dequeue(): T | undefined {
		return this.items.shift();
	}

	peek(): T | undefined {
		return this.items[0];
	}

	get size(): number {
		return this.items.length;
	}

	toArray(): T[] {
		return [...this.items];
	}

	clear(): void {
		this.items = [];
	}
}
