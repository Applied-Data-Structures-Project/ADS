import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import {
	buildCoachInstructions,
	buildGroqRequestBody,
	findCoachQuestion,
	isValidAucEmail,
	wordCount
} from '#lib/server/coachLogic.ts';
import { getServerEnv } from '#lib/server/env.ts';

function jsonError(status: number, error: string) {
	return json({ error }, { status });
}

export const POST: RequestHandler = async ({ request }) => {
	if (!isValidAucEmail(request.headers.get('x-user-email'))) {
		return jsonError(401, 'Sign in with your AUC account.');
	}

	let body: unknown;
	try {
		body = await request.json();
	} catch {
		return jsonError(400, 'The request is not valid JSON.');
	}

	if (!body || typeof body !== 'object' || Array.isArray(body)) {
		return jsonError(400, 'The request is not valid JSON.');
	}

	const payload = body as Record<string, unknown>;
	const questionKey = typeof payload.question === 'string' ? payload.question : '';
	const q = findCoachQuestion(questionKey);
	if (!q) {
		return jsonError(404, 'The coach does not know this question.');
	}

	const text = typeof payload.text === 'string' ? payload.text : '';
	const words = wordCount(text);
	if (words < 20) {
		return jsonError(400, 'Write at least a few sentences first, then ask for feedback.');
	}
	if (text.length > 40000) {
		return jsonError(413, 'The answer is too long for the coach.');
	}

	const apiKey = getServerEnv('GROQ_API_KEY');
	if (!apiKey) {
		return jsonError(503, 'The AI coach is not set up on this server.');
	}

	const model = getServerEnv('GROQ_MODEL', 'openai/gpt-oss-20b');
	const groqUrl = getServerEnv('GROQ_URL', 'https://api.groq.com/openai/v1/chat/completions');
	const instructions = buildCoachInstructions(q, words);

	let res: Response;
	try {
		res = await fetch(groqUrl, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
				Authorization: `Bearer ${apiKey}`
			},
			body: JSON.stringify(buildGroqRequestBody(model, instructions, text)),
			signal: AbortSignal.timeout(30_000)
		});
	} catch (err) {
		console.error('Groq request failed:', err);
		return jsonError(502, 'The AI coach is not available right now. Please try again.');
	}

	const rawAnswer = await res.text();
	if (res.status === 429) {
		return jsonError(502, 'The AI coach is busy (daily limit reached). Please try again later.');
	}

	let reply: unknown = null;
	try {
		reply = JSON.parse(rawAnswer);
	} catch {
		reply = null;
	}

	const replyObj =
		reply && typeof reply === 'object' ? (reply as Record<string, unknown>) : null;
	const choices = replyObj && Array.isArray(replyObj.choices) ? replyObj.choices : null;
	if (res.status !== 200 || !choices || choices.length === 0) {
		console.error(`Groq answered ${res.status}: ${rawAnswer.slice(0, 300)}`);
		return jsonError(502, 'The AI coach is not available right now. Please try again.');
	}

	const firstChoice = choices[0] as Record<string, unknown> | undefined;
	const message =
		firstChoice && firstChoice.message && typeof firstChoice.message === 'object'
			? (firstChoice.message as Record<string, unknown>)
			: null;
	const feedback = message && typeof message.content === 'string' ? message.content : '';
	if (feedback.length === 0) {
		console.error(`Groq answered with no text: ${rawAnswer.slice(0, 300)}`);
		return jsonError(502, 'The AI coach is not available right now. Please try again.');
	}

	return json({ feedback }, { status: 200 });
};
