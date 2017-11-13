/* This file contains a driver for a Floppy Disk Controller (FDC) using the
 * NEC PD765 chip.  The driver supports two operations: read a block and
 * write a block.  It accepts two messages, one for reading and one for
 * writing, both using message format m2 and with the same parameters:
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DISK_READ | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DISK_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   floppy_task:	main entry when system is brought up
 *
 *  Changes:
 *	27 october 1986 by Jakob Schripsema: fdc_results fixed for 8 MHz
 */

#include "../h/const.h"
#include "../h/type.h"
#include "../h/callnr.h"
#include "../h/com.h"
#include "../h/error.h"
#include "const.h"
#include "type.h"
#include "glo.h"
#include "proc.h"

/* I/O Ports used by floppy disk task. */
#define DOR            0x3F2	/* motor drive control bits */
#define FDC_STATUS     0x3F4	/* floppy disk controller status register */
#define FDC_DATA       0x3F5	/* floppy disk controller data register */
#define FDC_RATE       0x3F7	/* transfer rate register */
#define DMA_ADDR       0x004	/* port for low 16 bits of DMA address */
#define DMA_TOP        0x081	/* port for top 4 bits of 20-bit DMA addr */
#define DMA_COUNT      0x005	/* port for DMA count (count =  bytes - 1) */
#define DMA_M2         0x00C	/* DMA status port */
#define DMA_M1         0x00B	/* DMA status port */
#define DMA_INIT       0x00A	/* DMA init port */

/* Status registers returned as result of operation. */
#define ST0             0x00	/* status register 0 */
#define ST1             0x01	/* status register 1 */
#define ST2             0x02	/* status register 2 */
#define ST3             0x00	/* status register 3 (return by DRIVE_SENSE) */
#define ST_CYL          0x03	/* slot where controller reports cylinder */
#define ST_HEAD         0x04	/* slot where controller reports head */
#define ST_SEC          0x05	/* slot where controller reports sector */
#define ST_PCN          0x01	/* slot where controller reports present cyl */

/* Fields within the I/O ports. */
#define MASTER          0x80	/* used to see who is master */
#define DIRECTION       0x40	/* is FDC trying to read or write? */
#define CTL_BUSY        0x10	/* used to see when controller is busy */
#define CTL_ACCEPTING   0x80	/* bit pattern FDC gives when idle */
#define MOTOR_MASK      0xF0	/* these bits control the motors in DOR */
#define ENABLE_INT      0x0C	/* used for setting DOR port */
#define ST0_BITS        0xF8	/* check top 5 bits of seek status */
#define ST3_FAULT       0x80	/* if this bit is set, drive is sick */
#define ST3_WR_PROTECT  0x40	/* set when diskette is write protected */
#define ST3_READY       0x20	/* set when drive is ready */
#define TRANS_ST0       0x00	/* top 5 bits of ST0 for READ/WRITE */
#define SEEK_ST0        0x20	/* top 5 bits of ST0 for SEEK */
#define BAD_SECTOR      0x05	/* if these bits are set in ST1, recalibrate */
#define BAD_CYL         0x1F	/* if any of these bits are set, recalibrate */
#define WRITE_PROTECT   0x02	/* bit is set if diskette is write protected */
#define CHANGE          0xC0	/* value returned by FDC after reset */

/* Floppy disk controller command bytes. */
#define FDC_SEEK        0x0F	/* command the drive to seek */
#define FDC_READ        0xE6	/* command the drive to read */
#define FDC_WRITE       0xC5	/* command the drive to write */
#define FDC_SENSE       0x08	/* command the controller to tell its status */
#define FDC_RECALIBRATE 0x07	/* command the drive to go to cyl 0 */
#define FDC_SPECIFY     0x03	/* command the drive to accept params */

/* DMA channel commands. */
#define DMA_READ        0x46	/* DMA read opcode */
#define DMA_WRITE       0x4A	/* DMA write opcode */

/* Parameters for the disk drive. */
#define SECTOR_SIZE      512	/* physical sector size in bytes */
#define HC_SIZE         2400	/* # sectors on a high-capacity (1.2M) disk */
#define NR_HEADS        0x02	/* two heads (i.e., two tracks/cylinder) */
#define DTL             0xFF	/* determines data length (sector size) */
#define SPEC1           0xDF	/* first parameter to SPECIFY */
#define SPEC2           0x02	/* second parameter to SPECIFY */

#define MOTOR_OFF       3*HZ	/* how long to wait before stopping motor */

/* Error codes */
#define ERR_SEEK          -1	/* bad seek */
#define ERR_TRANSFER      -2	/* bad transfer */
#define ERR_STATUS        -3	/* something wrong when getting status */
#define ERR_RECALIBRATE   -4	/* recalibrate didn't work properly */
#define ERR_WR_PROTECT    -5	/* diskette is write protected */
#define ERR_DRIVE         -6	/* something wrong with a drive */

/* Miscellaneous. */
#define MOTOR_RUNNING   0xFF	/* message type for clock interrupt */
#define MAX_ERRORS        20	/* how often to try rd/wt before quitting */
#define MAX_RESULTS        8	/* max number of bytes controller returns */
#define NR_DRIVES          2	/* maximum number of drives */
#define DIVISOR          128	/* used for sector size encoding */
#define MAX_FDC_RETRY    100	/* max # times to try to output to FDC */
#define NT                 4	/* number of diskette/drive combinations */

/* Variables. */
PRIVATE struct floppy {		/* main drive struct, one entry per drive */
  int fl_opcode;		/* DISK_READ or DISK_WRITE */
  int fl_curcyl;		/* current cylinder */
  int fl_procnr;		/* which proc wanted this operation? */
  int fl_drive;			/* drive number addressed */
  int fl_cylinder;		/* cylinder number addressed */
  int fl_sector;		/* sector addressed */
  int fl_head;			/* head number addressed */
  int fl_count;			/* byte count */
  vir_bytes fl_address;		/* user virtual address */
  char fl_results[MAX_RESULTS];	/* the controller can give lots of output */
  char fl_calibration;		/* CALIBRATED or UNCALIBRATED */
  char fl_density;		/* 0 = 360K/360K; 1 = 360K/1.2M; 2= 1.2M/1.2M */
} floppy[NR_DRIVES];

#define UNCALIBRATED       0	/* drive needs to be calibrated at next use */
#define CALIBRATED         1	/* no calibration needed */

PRIVATE int motor_status;	/* current motor status is in 4 high bits */
PRIVATE int motor_goal;		/* desired motor status is in 4 high bits */
PRIVATE int prev_motor;		/* which motor was started last */
PRIVATE int need_reset;		/* set to 1 when controller must be reset */
PRIVATE int initialized;	/* set to 1 after first successful transfer */
PRIVATE int d;			/* diskette/drive combination */

PRIVATE message mess;		/* message buffer for in and out */

PRIVATE char len[] = {-1,0,1,-1,2,-1,-1,3,-1,-1,-1,-1,-1,-1,-1,4};
PRIVATE char interleave[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

/* Four combinations of diskette/drive are supported:
 * # Drive  diskette  Sectors  Tracks  Rotation Data-rate  Comment
 * 0  360K    360K      9       40     300 RPM  250 kbps   Standard PC DSDD
 * 1  720K    360K      9       40     300 RPM  250 kbps   Quad density PC
 * 2  1.2M    360K      9       40     360 RPM  300 kbps   PC disk in AT drive
 * 3  1.2M    1.2M     15       80     360 RPM  500 kbps   AT disk in AT drive
 */
PRIVATE int gap[NT]           = {0x2A, 0x2A, 0x23, 0x1B}; /* gap size */
PRIVATE int rate[NT]          = {0x02, 0x02, 0x01, 0x00}; /* 250,300,500 kbps*/
PRIVATE int nr_sectors[NT]    = {9,    9,    9,    15};   /* sectors/track */
PRIVATE int nr_blocks[NT]     = {720,  720,  720,  2400}; /* sectors/diskette*/
PRIVATE int steps_per_cyl[NT] = {1,    2,    2,    1};	  /* 2 = dbl step */
PRIVATE int mtr_setup[NT]     = {HZ/4,HZ/4,3*HZ/4,3*HZ/4};/* in ticks */

/*===========================================================================*
 *				floppy_task				     * 
 *===========================================================================*/
PUBLIC floppy_task()
{
/* Main program of the floppy disk driver task. */

  int r, caller, proc_nr;

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (TRUE) {
	/* First wait for a request to read or write a disk block. */
	receive(ANY, &mess);	/* get a request to do some work */
	if (mess.m_source < 0)
		panic("disk task got message from ", mess.m_source);
	caller = mess.m_source;
	proc_nr = mess.PROC_NR;

	/* Now carry out the work. */
	switch(mess.m_type) {
	    case DISK_READ:	r = do_rdwt(&mess);	break;
	    case DISK_WRITE:	r = do_rdwt(&mess);	break;
	    default:		r = EINVAL;		break;
	}

	/* Finally, prepare and send the reply message. */
	mess.m_type = TASK_REPLY;	
	mess.REP_PROC_NR = proc_nr;
	mess.REP_STATUS = r;	/* # of bytes transferred or error code */
	send(caller, &mess);	/* send reply to caller */
  }
}


/*===========================================================================*
 *				do_rdwt					     * 
 *===========================================================================*/
PRIVATE int do_rdwt(m_ptr)
message *m_ptr;			/* pointer to read or write message */
{
/* Carry out a read or write request from the disk. */
  register struct floppy *fp;
  int r, drive, errors, stop_motor();
  long block;

  /* Decode the message parameters. */
  drive = m_ptr->DEVICE;
  if (drive < 0 || drive >= NR_DRIVES) return(EIO);
  fp = &floppy[drive];		/* 'fp' points to entry for this drive */
  fp->fl_drive = drive;		/* save drive number explicitly */
  fp->fl_opcode = m_ptr->m_type;	/* DISK_READ or DISK_WRITE */
  if (m_ptr->POSITION % BLOCK_SIZE != 0) return(EINVAL);
  block = m_ptr->POSITION/SECTOR_SIZE;
  if (block >= HC_SIZE) return(EOF);	/* sector is beyond end of 1.2M disk */
  d = fp->fl_density;		/* diskette/drive combination */
  fp->fl_cylinder = (int) (block / (NR_HEADS * nr_sectors[d]));
  fp->fl_sector = (int) interleave[block % nr_sectors[d]];
  fp->fl_head = (int) (block % (NR_HEADS*nr_sectors[d]) )/nr_sectors[d];
  fp->fl_count = m_ptr->COUNT;
  fp->fl_address = (vir_bytes) m_ptr->ADDRESS;
  fp->fl_procnr = m_ptr->PROC_NR;
  if (fp->fl_count != BLOCK_SIZE) return(EINVAL);

  errors = 0;

  /* This loop allows a failed operation to be repeated. */
  while (errors <= MAX_ERRORS) {

	/* If a lot of errors occur when 'initialized' is 0, it probably
	 * means that we are trying at the wrong density.  Try another one.
	 * Increment 'errors' here since loop is aborted on error.
	 */
	errors++;		/* increment count once per loop cycle */
 	if (errors % (MAX_ERRORS/NT) == 0) {
 		d = (d + 1) % NT;	/* try next density */
 		fp->fl_density = d;
 		need_reset = 1;
	}
  	if (block >= nr_blocks[d]) continue;

	/* First check to see if a reset is needed. */
	if (need_reset) reset();

	/* Now set up the DMA chip. */
	dma_setup(fp);

	/* See if motor is running; if not, turn it on and wait */
	start_motor(fp);

	/* If we are going to a new cylinder, perform a seek. */
	r = seek(fp);
	if (r != OK) continue;	/* if error, try again */

	/* Perform the transfer. */
	r = transfer(fp);
	if (r == OK) break;	/* if successful, exit loop */
	if (r == ERR_WR_PROTECT) break;	/* retries won't help */

  }

  /* Start watch_dog timer to turn motor off in a few seconds */
  motor_goal = ENABLE_INT;	/* when timer goes off, kill all motors */
  clock_mess(MOTOR_OFF, stop_motor);
  if (r == OK && fp->fl_cylinder > 0) initialized = 1;	/* seek works */
  return(r == OK ? BLOCK_SIZE : EIO);
}

/*===========================================================================*
 *				dma_setup				     * 
 *===========================================================================*/
PRIVATE dma_setup(fp)
struct floppy *fp;		/* pointer to the drive struct */
{
/* The IBM PC can perform DMA operations by using the DMA chip.  To use it,
 * the DMA (Direct Memory Access) chip is loaded with the 20-bit memory address
 * to be read from or written to, the byte count minus 1, and a read or write
 * opcode.  This routine sets up the DMA chip.  Note that the chip is not
 * capable of doing a DMA across a 64K boundary (e.g., you can't read a 
 * 512-byte block starting at physical address 65520).
 */

  int mode, low_addr, high_addr, top_addr, low_ct, high_ct, top_end;
  vir_bytes vir, ct;
  phys_bytes user_phys;
  extern phys_bytes umap();

  mode = (fp->fl_opcode == DISK_READ ? DMA_READ : DMA_WRITE);
  vir = (vir_bytes) fp->fl_address;
  ct = (vir_bytes) fp->fl_count;
  user_phys = umap(proc_addr(fp->fl_procnr), D, vir, ct);
  low_addr  = (int) (user_phys >>  0) & BYTE;
  high_addr = (int) (user_phys >>  8) & BYTE;
  top_addr  = (int) (user_phys >> 16) & BYTE;
  low_ct  = (int) ( (ct - 1) >> 0) & BYTE;
  high_ct = (int) ( (ct - 1) >> 8) & BYTE;

  /* Check to see if the transfer will require the DMA address counter to
   * go from one 64K segment to another.  If so, do not even start it, since
   * the hardware does not carry from bit 15 to bit 16 of the DMA address.
   * Also check for bad buffer address.  These errors mean FS contains a bug.
   */
  if (user_phys == 0) panic("FS gave floppy disk driver bad addr", (int) vir);
  top_end = (int) (((user_phys + ct - 1) >> 16) & BYTE);
  if (top_end != top_addr) panic("Trying to DMA across 64K boundary", top_addr);

  /* Now set up the DMA registers. */
  lock();
  port_out(DMA_M2, mode);	/* set the DMA mode */
  port_out(DMA_M1, mode);	/* set it again */
  port_out(DMA_ADDR, low_addr);	/* output low-order 8 bits */
  port_out(DMA_ADDR, high_addr);/* output next 8 bits */
  port_out(DMA_TOP, top_addr);	/* output highest 4 bits */
  port_out(DMA_COUNT, low_ct);	/* output low 8 bits of count - 1 */
  port_out(DMA_COUNT, high_ct);	/* output high 8 bits of count - 1 */
  unlock();
  port_out(DMA_INIT, 2);	/* initialize DMA */
}


/*===========================================================================*
 *				start_motor				     * 
 *===========================================================================*/
PRIVATE start_motor(fp)
struct floppy *fp;		/* pointer to the drive struct */
{
/* Control of the floppy disk motors is a big pain.  If a motor is off, you
 * have to turn it on first, which takes 1/2 second.  You can't leave it on
 * all the time, since that would wear out the diskette.  However, if you turn
 * the motor off after each operation, the system performance will be awful.
 * The compromise used here is to leave it on for a few seconds after each
 * operation.  If a new operation is started in that interval, it need not be
 * turned on again.  If no new operation is started, a timer goes off and the
 * motor is turned off.  I/O port DOR has bits to control each of 4 drives.
 * Interrupts must be disabled temporarily to prevent clock interrupt from
 * turning off motors while we are testing the bits.
 */

  int motor_bit, running, send_mess();

  lock();			/* no interrupts while checking out motor */
  motor_bit = 1 << (fp->fl_drive + 4);	/* bit mask for this drive */
  motor_goal = motor_bit | ENABLE_INT | fp->fl_drive;
  if (motor_status & prev_motor) motor_goal |= prev_motor;
  running = motor_status & motor_bit;	/* nonzero if this motor is running */
  port_out(DOR, motor_goal);
  motor_status = motor_goal;
  prev_motor = motor_bit;	/* record motor started for next time */
  unlock();

  /* If the motor was already running, we don't have to wait for it. */
  if (running) return;			/* motor was already running */
  clock_mess(mtr_setup[d], send_mess);	/* motor was not running */
  receive(CLOCK, &mess);		/* wait for clock interrupt */
}


/*===========================================================================*
 *				stop_motor				     * 
 *===========================================================================*/
PRIVATE stop_motor()
{
/* This routine is called by the clock interrupt after several seconds have
 * elapsed with no floppy disk activity.  It checks to see if any drives are
 * supposed to be turned off, and if so, turns them off.
 */

  if ( (motor_goal & MOTOR_MASK) != (motor_status & MOTOR_MASK) ) {
	port_out(DOR, motor_goal);
	motor_status = motor_goal;
  }
}


/*===========================================================================*
 *				seek					     * 
 *===========================================================================*/
PRIVATE int seek(fp)
struct floppy *fp;		/* pointer to the drive struct */
{
/* Issue a SEEK command on the indicated drive unless the arm is already 
 * positioned on the correct cylinder.
 */

  int r;

  /* Are we already on the correct cylinder? */
  if (fp->fl_calibration == UNCALIBRATED)
	if (recalibrate(fp) != OK) return(ERR_SEEK);
  if (fp->fl_curcyl == fp->fl_cylinder) return(OK);

  /* No.  Wrong cylinder.  Issue a SEEK and wait for interrupt. */
  fdc_out(FDC_SEEK);		/* start issuing the SEEK command */
  fdc_out( (fp->fl_head << 2) | fp->fl_drive);
  fdc_out(fp->fl_cylinder * steps_per_cyl[d]);
  if (need_reset) return(ERR_SEEK);	/* if controller is sick, abort seek */
  receive(HARDWARE, &mess);

  /* Interrupt has been received.  Check drive status. */
  fdc_out(FDC_SENSE);		/* probe FDC to make it return status */
  r = fdc_results(fp);		/* get controller status bytes */
  if ( (fp->fl_results[ST0] & ST0_BITS) != SEEK_ST0) r = ERR_SEEK;
  if (fp->fl_results[ST1] != fp->fl_cylinder * steps_per_cyl[d]) r = ERR_SEEK;
  if (r != OK) 
	if (recalibrate(fp) != OK) return(ERR_SEEK);
  return(r);
}


/*===========================================================================*
 *				transfer				     * 
 *===========================================================================*/
PRIVATE int transfer(fp)
register struct floppy *fp;	/* pointer to the drive struct */
{
/* The drive is now on the proper cylinder.  Read or write 1 block. */

  int r, s, op;
  extern int olivetti;

  /* Never attempt a transfer if the drive is uncalibrated or motor is off. */
  if (fp->fl_calibration == UNCALIBRATED) return(ERR_TRANSFER);
  if ( ( (motor_status>>(fp->fl_drive+4)) & 1) == 0) return(ERR_TRANSFER);

  /* The PC-AT requires the date rate to be set to 250 or 500 kbps */
  if (pc_at) port_out(FDC_RATE, rate[d]);

  /* The command is issued by outputing 9 bytes to the controller chip. */
  op = (fp->fl_opcode == DISK_READ ? FDC_READ : FDC_WRITE);
  fdc_out(op);			/* issue the read or write command */
  fdc_out( (fp->fl_head << 2) | fp->fl_drive);
  fdc_out(fp->fl_cylinder);	/* tell controller which cylinder */
  fdc_out(fp->fl_head);		/* tell controller which head */
  fdc_out(fp->fl_sector);	/* tell controller which sector */
  fdc_out( (int) len[SECTOR_SIZE/DIVISOR]);	/* sector size */
  fdc_out(nr_sectors[d]);	/* tell controller how big a track is */
  fdc_out(gap[d]);		/* tell controller how big sector gap is */
  fdc_out(DTL);			/* tell controller about data length */

  /* Block, waiting for disk interrupt. */
  if (need_reset) return(ERR_TRANSFER);	/* if controller is sick, abort op */
  receive(HARDWARE, &mess);

  /* Get controller status and check for errors. */
  r = fdc_results(fp);
  if (r != OK) return(r);
  if ( (fp->fl_results[ST1] & BAD_SECTOR) || (fp->fl_results[ST2] & BAD_CYL) )
	fp->fl_calibration = UNCALIBRATED;
  if (fp->fl_results[ST1] & WRITE_PROTECT) {
	printf("Diskette in drive %d is write protected.\n", fp->fl_drive);
	return(ERR_WR_PROTECT);
  }
  if ((fp->fl_results[ST0] & ST0_BITS) != TRANS_ST0) return(ERR_TRANSFER);
  if (fp->fl_results[ST1] | fp->fl_results[ST2]) return(ERR_TRANSFER);

  /* Compare actual numbers of sectors transferred with expected number. */
  s =  (fp->fl_results[ST_CYL] - fp->fl_cylinder) * NR_HEADS * nr_sectors[d];
  s += (fp->fl_results[ST_HEAD] - fp->fl_head) * nr_sectors[d];
  s += (fp->fl_results[ST_SEC] - fp->fl_sector);
  if (s * SECTOR_SIZE != fp->fl_count) return(ERR_TRANSFER);
  return(OK);
}


/*===========================================================================*
 *				fdc_results				     * 
 *===========================================================================*/
PRIVATE int fdc_results(fp)
register struct floppy *fp;	/* pointer to the drive struct */
{
/* Extract results from the controller after an operation. */

  int i, j, status, ready;

  /* Loop, extracting bytes from FDC until it says it has no more. */
  for (i = 0; i < MAX_RESULTS; i++) {
	ready = FALSE;
	for (j = 0; j < MAX_FDC_RETRY; j++) {
		port_in(FDC_STATUS, &status);
		if (status & MASTER) {
			ready = TRUE;
			break;
		}
	}
	if (ready == FALSE) return(ERR_STATUS);	/* time out */

	if ((status & CTL_BUSY) == 0) return(OK);
	if ((status & DIRECTION) == 0) return(ERR_STATUS);
	port_in(FDC_DATA, &status);
	fp->fl_results[i] = status & BYTE;
  }

  /* FDC is giving back too many results. */
  need_reset = TRUE;		/* controller chip must be reset */
  return(ERR_STATUS);
}


/*===========================================================================*
 *				fdc_out					     * 
 *===========================================================================*/
PRIVATE fdc_out(val)
int val;			/* write this byte to floppy disk controller */
{
/* Output a byte to the controller.  This is not entirely trivial, since you
 * can only write to it when it is listening, and it decides when to listen.
 * If the controller refuses to listen, the FDC chip is given a hard reset.
 */

  int retries, r;

  if (need_reset) return;	/* if controller is not listening, return */
  retries = MAX_FDC_RETRY;

  /* It may take several tries to get the FDC to accept a command. */
  while (retries-- > 0) {
	port_in(FDC_STATUS, &r);
	r &= (MASTER | DIRECTION);	/* just look at bits 2 and 3 */
	if (r != CTL_ACCEPTING) continue;	/* FDC is not listening */
	port_out(FDC_DATA, val);
	return;
  }

  /* Controller is not listening.  Hit it over the head with a hammer. */
  need_reset = TRUE;
}


/*===========================================================================*
 *				recalibrate				     * 
 *===========================================================================*/
PRIVATE int recalibrate(fp)
register struct floppy *fp;	/* pointer tot he drive struct */
{
/* The floppy disk controller has no way of determining its absolute arm
 * position (cylinder).  Instead, it steps the arm a cylinder at a time and
 * keeps track of where it thinks it is (in software).  However, after a
 * SEEK, the hardware reads information from the diskette telling where the
 * arm actually is.  If the arm is in the wrong place, a recalibration is done,
 * which forces the arm to cylinder 0.  This way the controller can get back
 * into sync with reality.
 */

  int r;
  /* Issue the RECALIBRATE command and wait for the interrupt. */
  start_motor(fp);		/* can't recalibrate with motor off */
  fdc_out(FDC_RECALIBRATE);	/* tell drive to recalibrate itself */
  fdc_out(fp->fl_drive);	/* specify drive */
  if (need_reset) return(ERR_SEEK);	/* don't wait if controller is sick */
  receive(HARDWARE, &mess);	/* wait for interrupt message */

  /* Determine if the recalibration succeeded. */
  fdc_out(FDC_SENSE);		/* issue SENSE command to see where we are */
  r = fdc_results(fp);		/* get results of the SENSE command */
  fp->fl_curcyl = -1;		/* force a SEEK next time */
  if (r != OK ||		/* controller would not respond */
     (fp->fl_results[ST0]&ST0_BITS) != SEEK_ST0 || fp->fl_results[ST_PCN] !=0){
	/* Recalibration failed.  FDC must be reset. */
	need_reset = TRUE;
	fp->fl_calibration = UNCALIBRATED;
	return(ERR_RECALIBRATE);
  } else {
	/* Recalibration succeeded. */
	fp->fl_calibration = CALIBRATED;
	return(OK);
  }
}


/*===========================================================================*
 *				reset					     * 
 *===========================================================================*/
PRIVATE reset()
{
/* Issue a reset to the controller.  This is done after any catastrophe,
 * like the controller refusing to respond.
 */

  int i, r, status;
  register struct floppy *fp;
  /* Disable interrupts and strobe reset bit low. */
  need_reset = FALSE;
  lock();
  motor_status = 0;
  motor_goal = 0;
  port_out(DOR, 0);		/* strobe reset bit low */
  port_out(DOR, ENABLE_INT);	/* strobe it high again */
  unlock();			/* interrupts allowed again */
  receive(HARDWARE, &mess);	/* collect the RESET interrupt */

  /* Interrupt from the reset has been received.  Continue resetting. */
  fp = &floppy[0];		/* use floppy[0] for scratch */
  fp->fl_results[0] = 0;	/* this byte will be checked shortly */
  fdc_out(FDC_SENSE);		/* did it work? */
  r = fdc_results(fp);		/* get results */
  status = fp->fl_results[0] & BYTE;

  /* Tell FDC drive parameters. */
  fdc_out(FDC_SPECIFY);		/* specify some timing parameters */
  fdc_out(SPEC1);		/* step-rate and head-unload-time */
  fdc_out(SPEC2);		/* head-load-time and non-dma */

  for (i = 0; i < NR_DRIVES; i++) floppy[i].fl_calibration = UNCALIBRATED;
}


/*===========================================================================*
 *				clock_mess				     * 
 *===========================================================================*/
PRIVATE clock_mess(ticks, func)
int ticks;			/* how many clock ticks to wait */
int (*func)();			/* function to call upon time out */
{
/* Send the clock task a message. */

  mess.m_type = SET_ALARM;
  mess.CLOCK_PROC_NR = FLOPPY;
  mess.DELTA_TICKS = ticks;
  mess.FUNC_TO_CALL = func;
  sendrec(CLOCK, &mess);
}


/*===========================================================================*
 *				send_mess				     * 
 *===========================================================================*/
PRIVATE send_mess()
{
/* This routine is called when the clock task has timed out on motor startup.*/

  mess.m_type = MOTOR_RUNNING;
  send(FLOPPY, &mess);
}
