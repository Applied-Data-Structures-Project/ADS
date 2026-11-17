// Role mapping for Microsoft (Entra ID) sign-in.
//
// AUC IT defines app roles on the app registration and assigns them in Entra.
// The role arrives as the `roles` claim in the ID token. Students need no
// assignment: an account with no staff role is treated as a student.

export type Role = 'student' | 'admin' | 'faculty';

const AUC_DOMAIN = '@aucegypt.edu';

/** True for an AUC Microsoft account (@aucegypt.edu, any casing). */
export function isAucEmail(email: string | null | undefined): boolean {
	return typeof email === 'string' && email.trim().toLowerCase().endsWith(AUC_DOMAIN);
}

/** Map the token's `roles` claim to an app role. Admin wins if both are present. */
export function roleFromRoles(roles: string[] | undefined): Role {
	const set = new Set((roles ?? []).map((role) => role.toLowerCase()));
	if (set.has('admin')) return 'admin';
	if (set.has('faculty')) return 'faculty';
	return 'student';
}

/** Where each role lands after sign-in. */
export function roleHome(role: Role): string {
	if (role === 'admin') return '/admin';
	if (role === 'faculty') return '/doctor';
	return '/apply';
}

/** Human label for the role, used in the app shell. */
export function roleLabel(role: Role): string {
	if (role === 'admin') return 'IPSO Administrator';
	if (role === 'faculty') return 'AUC Faculty';
	return 'Student';
}

/** Only allow same-site destinations in `?next=`, never an open redirect. */
export function isSafeNext(path: string | null | undefined): path is string {
	return (
		typeof path === 'string' &&
		path.startsWith('/') &&
		!path.startsWith('//') &&
		!path.startsWith('/\\')
	);
}
