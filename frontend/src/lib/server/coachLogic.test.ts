import { describe, expect, it } from 'vitest';
import {
	buildCoachInstructions,
	buildGroqRequestBody,
	findCoachQuestion,
	isValidAucEmail,
	wordCount
} from './coachLogic.ts';
import {
	computeRelayToken,
	isLoopbackAddress,
	parseEnvContent,
	sha256Hex,
	verifyRelayToken
} from './env.ts';

describe('coachLogic', () => {
	it('counts words identically to ApplicationService::wordCount', () => {
		expect(wordCount('')).toBe(0);
		expect(wordCount('   \n\t\r  ')).toBe(0);
		expect(wordCount('one two  three\nfour\tfive\rsix')).toBe(6);
		expect(wordCount('Hello world.')).toBe(2);
	});

	it('validates @aucegypt.edu email headers case-insensitively', () => {
		expect(isValidAucEmail(null)).toBe(false);
		expect(isValidAucEmail('')).toBe(false);
		expect(isValidAucEmail('user@gmail.com')).toBe(false);
		expect(isValidAucEmail('Salma@AUCEgypt.edu')).toBe(true);
		expect(isValidAucEmail('dr.hany@aucegypt.edu')).toBe(true);
	});

	it('looks up known coach questions and builds matching instructions', () => {
		expect(findCoachQuestion('unknown')).toBeNull();

		const goals = findCoachQuestion('qGoals');
		expect(goals).not.toBeNull();
		const goalsPrompt = buildCoachInstructions(goals!, 342);
		expect(goalsPrompt).toContain('The required length is 300-500 words; this answer has 342 words.');
		expect(goalsPrompt).toContain('How does the program align with your academic goals?');

		const adapt = findCoachQuestion('qAdaptability');
		expect(adapt).not.toBeNull();
		const adaptPrompt = buildCoachInstructions(adapt!, 315);
		expect(adaptPrompt).toContain('The required length is 300+ words; this answer has 315 words.');

		const teamwork = findCoachQuestion('qTeamwork');
		expect(teamwork).not.toBeNull();
		const teamworkPrompt = buildCoachInstructions(teamwork!, 120);
		expect(teamworkPrompt).not.toContain('The required length is');
	});

	it('adds reasoning_effort only for gpt-oss models', () => {
		const ossBody = buildGroqRequestBody('openai/gpt-oss-20b', 'sys', 'essay');
		expect(ossBody.reasoning_effort).toBe('low');

		const llamaBody = buildGroqRequestBody('llama-3.3-70b-versatile', 'sys', 'essay');
		expect('reasoning_effort' in llamaBody).toBe(false);
	});
});

describe('server env & relay token helpers', () => {
	it('parses KEY=value lines with comments, CRLF, quotes, and embedded =', () => {
		const raw = [
			'# comment line',
			'',
			'GROQ_API_KEY="gsk_abc=123"\r',
			"SMTP_USER=aucreccomendationsystem@gmail.com\r",
			'SMTP_URL=smtps://smtp.gmail.com:465',
			'INVALID_LINE_WITHOUT_EQUALS'
		].join('\n');

		const parsed = parseEnvContent(raw);
		expect(parsed).toEqual({
			GROQ_API_KEY: 'gsk_abc=123',
			SMTP_USER: 'aucreccomendationsystem@gmail.com',
			SMTP_URL: 'smtps://smtp.gmail.com:465'
		});
	});

	it('computes standard FIPS 180-4 SHA-256 and verifies relay tokens safely', () => {
		expect(sha256Hex('')).toBe(
			'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
		);

		const derivedFromCreds = computeRelayToken('', 'test@aucegypt.edu', 'app-pass');
		expect(derivedFromCreds).toBe(sha256Hex('ads-email-relay:test@aucegypt.edu:app-pass'));
		expect(verifyRelayToken(derivedFromCreds, derivedFromCreds)).toBe(true);
		expect(verifyRelayToken('wrong', derivedFromCreds)).toBe(false);
		expect(verifyRelayToken(null, derivedFromCreds)).toBe(false);

		const fromSecret = computeRelayToken('custom-secret', 'test@aucegypt.edu', 'app-pass');
		expect(fromSecret).toBe(sha256Hex('ads-email-relay-secret:custom-secret'));
		expect(computeRelayToken('', '', '')).toBe('');
	});

	it('recognizes loopback addresses only', () => {
		expect(isLoopbackAddress('127.0.0.1')).toBe(true);
		expect(isLoopbackAddress('::1')).toBe(true);
		expect(isLoopbackAddress('::ffff:127.0.0.1')).toBe(true);
		expect(isLoopbackAddress('localhost')).toBe(true);
		expect(isLoopbackAddress('192.168.1.10')).toBe(false);
		expect(isLoopbackAddress('')).toBe(false);
	});
});
