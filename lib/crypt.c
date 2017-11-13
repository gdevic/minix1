char *crypt(pw, salt)
char *pw, *salt;
{
	static char buf[14];
	register char bits[67];
	register int i;
	register int j, rot;

	for (i=0; i < 67; i++)
		bits[i] = 0;
	if (salt[1] == 0)
		salt[1] = salt[0];
	rot = (salt[1] * 4 - salt[0]) % 128;
	for (i=0; *pw && i < 8; i++) {
		for (j=0; j < 7; j++)
			bits[i+j*8] = (*pw & (1 << j) ? 1 : 0);
		bits[i+56] = (salt[i / 4] & (1 << (i % 4)) ? 1 : 0);
		pw++;
	}
	bits[64] = (salt[0] & 1 ? 1 : 0);
	bits[65] = (salt[1] & 1 ? 1 : 0);
	bits[66] = (rot & 1 ? 1 : 0);
	while (rot--) {
		for (i=65; i >= 0; i--)
			bits[i+1] = bits[i];
		bits[0] = bits[66];
	}
	for (i=0; i < 12; i++) {
		buf[i+2] = 0;
		for (j=0; j < 6; j++)
			buf[i+2] |= (bits[i*6+j] ? (1 << j) : 0);
		buf[i+2] += 48;
		if (buf[i+2] > '9') buf[i+2] += 7;
		if (buf[i+2] > 'Z') buf[i+2] += 6;
	}
	buf[0] = salt[0];
	buf[1] = salt[1];
	buf[13] = '\0';
	return(buf);
}
