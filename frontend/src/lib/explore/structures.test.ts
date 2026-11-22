import { describe, expect, it } from 'vitest';
import { MaxHeap, mergeSort, MinHeap, NotificationQueue, topK } from './structures.ts';

describe('mergeSort', () => {
	it('sorts numbers without changing the input', () => {
		const input = [5, 1, 4, 2, 3];
		const sorted = mergeSort(input, (a, b) => a - b);

		expect(sorted).toEqual([1, 2, 3, 4, 5]);
		expect(input).toEqual([5, 1, 4, 2, 3]);
	});

	it('is stable: equal keys keep their original order', () => {
		const input = [
			{ id: 1, score: 10 },
			{ id: 2, score: 20 },
			{ id: 3, score: 10 },
			{ id: 4, score: 20 }
		];
		const sorted = mergeSort(input, (a, b) => a.score - b.score);

		expect(sorted.map((item) => item.id)).toEqual([1, 3, 2, 4]);
	});

	it('handles empty and single-item lists', () => {
		expect(mergeSort([], (a: number, b: number) => a - b)).toEqual([]);
		expect(mergeSort([7], (a, b) => a - b)).toEqual([7]);
	});
});

describe('MinHeap', () => {
	it('pops the smallest value first', () => {
		const heap = new MinHeap<number>((a, b) => a - b);
		for (const value of [9, 4, 7, 1, 5]) heap.push(value);

		const popped: number[] = [];
		while (heap.size > 0) popped.push(heap.pop() as number);

		expect(popped).toEqual([1, 4, 5, 7, 9]);
		expect(heap.pop()).toBeUndefined();
	});
});

describe('MaxHeap', () => {
	it('pops the largest value first', () => {
		const heap = new MaxHeap<number>((a, b) => a - b);
		for (const value of [9, 4, 7, 1, 5]) heap.push(value);

		const popped: number[] = [];
		while (heap.size > 0) popped.push(heap.pop() as number);

		expect(popped).toEqual([9, 7, 5, 4, 1]);
		expect(heap.peek()).toBeUndefined();
	});
});

describe('topK', () => {
	it('returns the k highest scores, best first', () => {
		const items = [
			{ name: 'a', score: 10 },
			{ name: 'b', score: 40 },
			{ name: 'c', score: 25 },
			{ name: 'd', score: 35 }
		];
		expect(topK(items, (item) => item.score, 3).map((item) => item.name)).toEqual(['b', 'd', 'c']);
	});

	it('keeps the earlier item first when scores tie', () => {
		const items = [
			{ name: 'first', score: 50 },
			{ name: 'second', score: 50 }
		];
		expect(topK(items, (item) => item.score, 2).map((item) => item.name)).toEqual([
			'first',
			'second'
		]);
	});

	it('returns everything when k is larger than the list, and nothing when k is 0', () => {
		expect(topK([1, 2], (n) => n, 10)).toEqual([2, 1]);
		expect(topK([1, 2], (n) => n, 0)).toEqual([]);
	});
});

describe('NotificationQueue', () => {
	it('is first in, first out', () => {
		const queue = NotificationQueue.from(['oldest', 'middle', 'newest']);

		expect(queue.size).toBe(3);
		expect(queue.peek()).toBe('oldest');
		expect(queue.dequeue()).toBe('oldest');
		expect(queue.toArray()).toEqual(['middle', 'newest']);
		expect(queue.dequeue()).toBe('middle');
		expect(queue.dequeue()).toBe('newest');
		expect(queue.dequeue()).toBeUndefined();
	});

	it('enqueues at the back', () => {
		const queue = new NotificationQueue<string>();
		queue.enqueue('one');
		queue.enqueue('two');
		expect(queue.toArray()).toEqual(['one', 'two']);
	});
});
