// Zero-dependency PDF 1.4 generator for the official AUC
// "Draft List of Courses for Study Abroad" document.

export interface CoursePlanPdfEntry {
	hostCourseCode: string;
	hostCourseTitle: string;
	aucEquivalent: string;
}

export interface CoursePlanPdfSection {
	universityName: string;
	studyTerm: string;
	courses: CoursePlanPdfEntry[];
}

export interface CoursePlanPdfInput {
	studentName: string;
	aucId: string;
	studyTerm: string;
	sections: CoursePlanPdfSection[];
}

interface TextLine {
	text: string;
	font: 'F1' | 'F2'; // F1 = Helvetica, F2 = Helvetica-Bold
	size: number;
	indent?: number;
	spaceBefore?: number;
	spaceAfter?: number;
	ruleAfter?: boolean;
}

const PAGE_WIDTH = 612; // US Letter (8.5 in)
const PAGE_HEIGHT = 792; // US Letter (11 in)
const MARGIN_X = 54;
const MARGIN_TOP = 54;
const MARGIN_BOTTOM = 54;
const CONTENT_WIDTH = PAGE_WIDTH - MARGIN_X * 2;

const INSTRUCTIONS_TEXT =
	'This draft list of courses is a preliminary document intended to guide your research and planning for your study abroad experience. You must consult with your academic advisor before finalizing your course selection. After being accepted into the program, you must complete a course approval form with the registrar\'s office. This draft list is not final and may (and will probably) be subject to change.';

const FOOTER_NOTE =
	'Note: The final course selection will depend on availability at the host institution and your ability to register for the courses.';

/** Sanitize text to WinAnsi/ASCII printable characters and escape PDF string delimiters. */
function pdfText(value: string): string {
	const normalized = (value || '')
		.replace(/[\u2018\u2019]/g, "'")
		.replace(/[\u201C\u201D]/g, '"')
		.replace(/[\u2013\u2014]/g, '-')
		.replace(/\u2026/g, '...')
		.replace(/\u00A0/g, ' ')
		.replace(/[^\x20-\x7E]/g, ' ');
	return normalized.replace(/\\/g, '\\\\').replace(/\(/g, '\\(').replace(/\)/g, '\\)');
}

/** Approximate average character width for Helvetica at a given font size. */
function wrapText(text: string, fontSize: number, maxWidth: number): string[] {
	const cleaned = (text || '').trim().replace(/\s+/g, ' ');
	if (!cleaned) return [''];
	const approxCharWidth = fontSize * 0.52;
	const maxChars = Math.max(20, Math.floor(maxWidth / approxCharWidth));

	const words = cleaned.split(' ');
	const lines: string[] = [];
	let current = '';

	for (const word of words) {
		if (!current) {
			if (word.length <= maxChars) {
				current = word;
			} else {
				for (let i = 0; i < word.length; i += maxChars) {
					lines.push(word.slice(i, i + maxChars));
				}
			}
		} else if (current.length + 1 + word.length <= maxChars) {
			current += ' ' + word;
		} else {
			lines.push(current);
			current = word;
		}
	}
	if (current) lines.push(current);
	return lines;
}

function buildDocumentLines(input: CoursePlanPdfInput): TextLine[] {
	const lines: TextLine[] = [];
	const hostNames = input.sections
		.map((s) => s.universityName.trim())
		.filter(Boolean)
		.join(', ');

	lines.push({
		text: 'Draft List of Courses for Study Abroad',
		font: 'F2',
		size: 16,
		spaceAfter: 10
	});

	lines.push({
		text: `Student Name: ${input.studentName.trim() || '______________________________'}`,
		font: 'F1',
		size: 10.5,
		spaceAfter: 3
	});
	lines.push({
		text: `AUC ID Number: ${input.aucId.trim() || '______________________________'}`,
		font: 'F1',
		size: 10.5,
		spaceAfter: 3
	});
	for (const wrapped of wrapText(
		`Host Institution(s) (Applying to/Intending): ${hostNames || '______________________________'}`,
		10.5,
		CONTENT_WIDTH
	)) {
		lines.push({ text: wrapped, font: 'F1', size: 10.5, spaceAfter: 2 });
	}
	lines.push({
		text: `Study Abroad Term: ${input.studyTerm.trim() || 'Fall 2027'}`,
		font: 'F1',
		size: 10.5,
		spaceAfter: 10,
		ruleAfter: true
	});

	lines.push({
		text: 'Course Planning',
		font: 'F2',
		size: 12.5,
		spaceBefore: 6,
		spaceAfter: 4
	});
	lines.push({
		text: 'Instructions:',
		font: 'F2',
		size: 10,
		spaceAfter: 2
	});
	for (const wrapped of wrapText(INSTRUCTIONS_TEXT, 9.5, CONTENT_WIDTH)) {
		lines.push({ text: wrapped, font: 'F1', size: 9.5, spaceAfter: 2 });
	}

	const sections =
		input.sections.length > 0
			? input.sections
			: [{ universityName: 'Host Institution', studyTerm: input.studyTerm, courses: [] }];

	for (const section of sections) {
		if (sections.length > 1) {
			lines.push({
				text: `Host Institution: ${section.universityName}${section.studyTerm ? ` (${section.studyTerm})` : ''}`,
				font: 'F2',
				size: 11.5,
				spaceBefore: 12,
				spaceAfter: 4,
				ruleAfter: true
			});
		} else {
			lines.push({ text: '', font: 'F1', size: 6, spaceAfter: 4 });
		}

		// Always show at least 5 numbered slots to match the official AUC template.
		const totalSlots = Math.max(5, section.courses.length);
		for (let i = 0; i < totalSlots; i++) {
			const c = section.courses[i];
			const title = c?.hostCourseTitle?.trim() || '[Course Title]';
			const code = c?.hostCourseCode?.trim() || '[Course Code]';
			const aucEq = c?.aucEquivalent?.trim() || '[AUC Course Equivalent, if known]';

			const titleLines = wrapText(`${i + 1}. Host University Course Title: ${title}`, 10, CONTENT_WIDTH);
			titleLines.forEach((line, idx) => {
				lines.push({
					text: line,
					font: idx === 0 ? 'F2' : 'F1',
					size: 10,
					indent: idx === 0 ? 0 : 14,
					spaceBefore: idx === 0 ? 6 : 0,
					spaceAfter: 2
				});
			});

			for (const line of wrapText(`Host University Course Code: ${code}`, 10, CONTENT_WIDTH - 14)) {
				lines.push({ text: line, font: 'F1', size: 10, indent: 14, spaceAfter: 2 });
			}

			const eqLines = wrapText(`AUC Equivalent Course: ${aucEq}`, 10, CONTENT_WIDTH - 14);
			eqLines.forEach((line, idx) => {
				lines.push({
					text: line,
					font: 'F1',
					size: 10,
					indent: 14,
					spaceAfter: idx === eqLines.length - 1 ? 6 : 2,
					ruleAfter: idx === eqLines.length - 1
				});
			});
		}
	}

	for (const wrapped of wrapText(FOOTER_NOTE, 9.5, CONTENT_WIDTH)) {
		lines.push({ text: wrapped, font: 'F2', size: 9.5, spaceBefore: 6, spaceAfter: 2 });
	}

	return lines;
}

/** Build a valid multi-page PDF binary matching AUC's official Draft List of Courses template. */
export function buildCoursePlanPdf(input: CoursePlanPdfInput): Uint8Array {
	const lines = buildDocumentLines(input);
	const pageStreams: string[] = [];

	let commands: string[] = [];
	let y = PAGE_HEIGHT - MARGIN_TOP;

	const flushPage = () => {
		pageStreams.push(commands.join('\n'));
		commands = [];
		y = PAGE_HEIGHT - MARGIN_TOP;
	};

	for (const line of lines) {
		const leading = line.size * 1.35;
		const needed = (line.spaceBefore ?? 0) + leading + (line.spaceAfter ?? 0) + (line.ruleAfter ? 6 : 0);

		if (y - needed < MARGIN_BOTTOM && commands.length > 0) {
			flushPage();
		}

		y -= line.spaceBefore ?? 0;
		y -= leading;

		if (line.text) {
			const x = MARGIN_X + (line.indent ?? 0);
			commands.push(`BT /${line.font} ${line.size} Tf ${x.toFixed(1)} ${y.toFixed(1)} Td (${pdfText(line.text)}) Tj ET`);
		}

		if (line.ruleAfter) {
			const ruleY = y - 4;
			commands.push(`0.82 G 0.6 w ${MARGIN_X} ${ruleY.toFixed(1)} m ${PAGE_WIDTH - MARGIN_X} ${ruleY.toFixed(1)} l S 0 G`);
			y -= 6;
		}

		y -= line.spaceAfter ?? 0;
	}

	if (commands.length > 0 || pageStreams.length === 0) {
		flushPage();
	}

	// Assemble PDF 1.4 objects:
	// 1: Catalog, 2: Pages, 3: Helvetica, 4: Helvetica-Bold,
	// then pairs of (Page, ContentStream) starting at object 5.
	const objects: string[] = [];
	const pageObjectIds: number[] = [];

	const firstPageObjId = 5;
	for (let i = 0; i < pageStreams.length; i++) {
		pageObjectIds.push(firstPageObjId + i * 2);
	}

	objects.push('<< /Type /Catalog /Pages 2 0 R >>');
	objects.push(
		`<< /Type /Pages /Kids [${pageObjectIds.map((id) => `${id} 0 R`).join(' ')}] /Count ${pageObjectIds.length} >>`
	);
	objects.push('<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>');
	objects.push('<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>');

	const encoder = new TextEncoder();
	for (let i = 0; i < pageStreams.length; i++) {
		const stream = pageStreams[i];
		const streamBytes = encoder.encode(stream);
		const contentObjId = firstPageObjId + i * 2 + 1;
		objects.push(
			`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_WIDTH} ${PAGE_HEIGHT}] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents ${contentObjId} 0 R >>`
		);
		objects.push(`<< /Length ${streamBytes.length} >>\nstream\n${stream}\nendstream`);
	}

	const chunks: string[] = ['%PDF-1.4\n'];
	const offsets: number[] = [0]; // object 0
	let byteOffset = encoder.encode(chunks[0]).length;

	for (let i = 0; i < objects.length; i++) {
		offsets.push(byteOffset);
		const block = `${i + 1} 0 obj\n${objects[i]}\nendobj\n`;
		chunks.push(block);
		byteOffset += encoder.encode(block).length;
	}

	const xrefOffset = byteOffset;
	let xref = `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`;
	for (let i = 1; i < offsets.length; i++) {
		xref += `${String(offsets[i]).padStart(10, '0')} 00000 n \n`;
	}
	const trailer = `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xrefOffset}\n%%EOF\n`;
	chunks.push(xref, trailer);

	return encoder.encode(chunks.join(''));
}

export function buildCoursePlanFilename(studentName: string, aucId: string): string {
	const safeName = (studentName || 'Student').trim().replace(/[^A-Za-z0-9 _-]+/g, '').replace(/\s+/g, '_') || 'Student';
	const safeId = (aucId || 'AUC').trim().replace(/[^A-Za-z0-9_-]+/g, '') || 'AUC';
	return `${safeName}-${safeId}-Draft-Courses.pdf`;
}
