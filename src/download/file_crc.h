#ifndef FILE_CRC_H
#define FILE_CRC_H

/*
 * Verifies if a file matches the expected CRC-32 checksum
 *
 * @param filename Path to the file to verify
 * @param expected_crc The expected CRC-32 value to compare against
 * @return 0 if CRC matches, -1 if error or mismatch
 */
int ad_verify_file_crc(const char *filename, const char *expected_crc_str);

#endif /* FILE_CRC_H */