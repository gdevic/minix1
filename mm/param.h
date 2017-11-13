/* The following names are synonyms for the variables in the input message. */
#define addr		mm_in.m1_p1
#define exec_name	mm_in.m1_p1
#define exec_len	mm_in.m1_i1
#define func		mm_in.m6_f1
#define grpid		(gid) mm_in.m1_i1
#define kill_sig	mm_in.m1_i2
#define namelen		mm_in.m1_i1
#define pid		mm_in.m1_i1
#define seconds		mm_in.m1_i1
#define sig		mm_in.m6_i1
#define stack_bytes	mm_in.m1_i2
#define stack_ptr	mm_in.m1_p2
#define status		mm_in.m1_i1
#define usr_id		(uid) mm_in.m1_i1

/* The following names are synonyms for the variables in the output message. */
#define reply_type      mm_out.m_type
#define reply_i1        mm_out.m2_i1
#define reply_p1        mm_out.m2_p1
