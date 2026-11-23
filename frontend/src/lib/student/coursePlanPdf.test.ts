import { describe, expect, it } from 'vitest';
import { buildCoursePlanFilename, buildCoursePlanPdf } from './coursePlanPdf.ts';

describe('coursePlanPdf generator', () => {
	it('produces a valid PDF 1.4 binary matching the AUC Draft List template', () => {
		const bytes = buildCoursePlanPdf({
			studentName: 'Salma Abdelrahman',
			aucId: '900219004',
			studyTerm: 'Fall 2027',
			sections: [
				{
					universityName: 'Aarhus University',
					studyTerm: 'Fall 2027',
					courses: [
						{
							hostCourseCode: 'ALG BIO',
							hostCourseTitle: 'Algorithms in Bioinformatics',
							aucEquivalent: 'BIOT 5206 Fundamentals of Bioinformatics, cr.hrs.: 3'
						}
					]
				}
			]
		});

		const text = new TextDecoder().decode(bytes);
		expect(text.startsWith('%PDF-1.4\n')).toBe(true);
		expect(text.includes('xref\n')).toBe(true);
		expect(text.trimEnd().endsWith('%%EOF')).toBe(true);
		expect(text).toContain('Draft List of Courses for Study Abroad');
		expect(text).toContain('Salma Abdelrahman');
		expect(text).toContain('900219004');
		expect(text).toContain('Aarhus University');
		expect(text).toContain('Algorithms in Bioinformatics');
		expect(text).toContain('5. Host University Course Title');
	});

	it('paginates cleanly when multiple universities and many courses are included', () => {
		const manyCourses = Array.from({ length: 12 }, (_, i) => ({
			hostCourseCode: `CS ${300 + i}`,
			hostCourseTitle: `Advanced Computer Science Topic ${i + 1}`,
			aucEquivalent: `CSCE ${3100 + i} Elective, cr.hrs.: 3`
		}));

		const bytes = buildCoursePlanPdf({
			studentName: 'Mohamed Greida',
			aucId: '900221100',
			studyTerm: 'Spring 2028',
			sections: [
				{ universityName: 'Bocconi University', studyTerm: 'Spring 2028', courses: manyCourses },
				{ universityName: 'Aarhus University', studyTerm: 'Spring 2028', courses: manyCourses }
			]
		});

		const text = new TextDecoder().decode(bytes);
		const pageCountMatch = text.match(/\/Count (\d+)/);
		expect(pageCountMatch).not.toBeNull();
		expect(Number(pageCountMatch?.[1])).toBeGreaterThan(1);
	});

	it('creates a clean PDF filename from student name and AUC ID', () => {
		expect(buildCoursePlanFilename('Salma Abdelrahman', '900219004')).toBe(
			'Salma_Abdelrahman-900219004-Draft-Courses.pdf'
		);
	});
});
