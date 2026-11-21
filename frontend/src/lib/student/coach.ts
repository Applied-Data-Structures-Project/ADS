// The AI essay coach (G3). Sends one answer to the C++ server (POST /api/coach),
// which asks Groq for tips. The coach never rewrites the essay.

export type CoachQuestion =
	| 'qPreparedness'
	| 'qGoals'
	| 'qAdaptability'
	| 'qTeamwork'
	| 'qReflection'
	| 'qFinance';

/** Returns the coach's tips, or throws an Error with a sentence the page can show. */
export async function getFeedback(question: CoachQuestion, text: string, email: string): Promise<string> {
	let res: Response;
	try {
		res = await fetch('/api/coach', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json', 'X-User-Email': email },
			body: JSON.stringify({ question, text })
		});
	} catch {
		throw new Error('The coach is not reachable. Is the C++ server running?');
	}
	const data = await res.json().catch(() => null);
	if (!res.ok) throw new Error(data?.error ?? 'The coach is not available right now.');
	return String(data?.feedback ?? '');
}