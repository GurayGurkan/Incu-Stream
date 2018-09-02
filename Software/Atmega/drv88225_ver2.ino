// Compatible Incu=Stream Interface
// 11.07.2018

// 1408.2018: Scanning Types 1,2 and 3 introduced
// Type 1: Snake Like Sacnning
// Type 2: Single Y axis Reset, X axis bouncing
// Type 3: Single Y axis Reset, Xaxis reset and bouncing
// StepXYZ() function changed for Long Type
// Plate Offset, aralik are now in Long Type
// 1/32 Stepping is used for every movement except Reset2Origin()

//*****************************
//      DRV8825 Enable pins
//*****************************
int Xen = 8;
int Yen = 9;
int Zen = 10;

//*****************************
//      (Common) DRV8825
//  Direction, Step and Mode pins
//*****************************
int XYZdir = 11; // 1->Reset Yonu, 0-> Uzaklasma
int XYZstep = 12;
int XYZSlowMode = 13;
int Positions[] = {0, 0, 0};

//*****************************
//      More Externals
//  X,Y Limit Switches,Buzzer, LED
//*****************************
int BEEP_pin = 4;
int LEDPWM_pin = 3; //PWM 2kHz
int SWITCH_x_pin = 6;
int SWITCH_y_pin = 7 ;

int TRAY_pin = 5;


// ***********************************
//  Well Plates vs. Steps required for TPP
//**************************************
unsigned long aralik[5] = {30016, 19968, 15232, 6400, 7296}; // in "1/32" Steps

// OFFSETS for A-1 Well Location
unsigned long X_offset[5] = {0, 0, 55200, 0, 53376};
unsigned long Y_offset[5] = {3200, 3200, 15200, 3200, 14304};

// SUB- GRID CAMERA SHIFT steps for non-overlap
int shiftX = 288;//288;// 11x32
int shiftY = 544;// 18x32

// SUB-GRID scan row and column numbers
int rows;
int cols;

int zstep = 320;
int zstep_fine = 40;

// LIVE VIEW PARAMETERS
int xstep = 288;
int ystep = 580;

int resetsteps = 896; // in 1/32 drive mode

int deltaX;
int deltaY;

unsigned long Zcurrent = 0;
unsigned long  Xcurrent = 0;
unsigned long Ycurrent = 0;

#define SLOWMODE1 400
#define SLOWMODE2 20

int PFullDrive = 500 ; //Delay Microseconds 600 idi
int PSlowDrive = SLOWMODE1; //Delay Microseconds


/*
 *************************************
      BACKLASH CORRECTION
 *************************************
*/
byte lastdir[3] = {0, 0, 0}; // last directions
unsigned long lashes[3] = {0 , 0, 620};// 1/32 drive

boolean finished = false;
boolean started = false;
boolean grid_selected = false;

int plate_type;
int x_i, y_i;
int x_p, y_p;
int grid_count;
bool at_wellcenter = false;

void setup() {

  DDRB |= 0x3F;
  pinMode(BEEP_pin, OUTPUT);
  pinMode(LEDPWM_pin, OUTPUT);
  pinMode(SWITCH_x_pin, INPUT_PULLUP);//0 if at limit
  pinMode(SWITCH_y_pin, INPUT_PULLUP);

  digitalWrite(Xen, HIGH);
  digitalWrite(Yen, HIGH);
  digitalWrite(Zen, HIGH);

  delay(1000);

  delay(10);


  beep(BEEP_pin, 50, 1); // POWER ON ALERT


  // **************************************************************
  //               SETUP SERIAL and TRANSMIT DEVICE ACK
  // **************************************************************
  Serial.begin(9600);
  delay(100);

  Serial.print("INCU"); // Device ACK
  delay(100);
  beep(BEEP_pin, 400, 1);
  // **************************************************************
  //               END OF SERIAL SETUP and TRANSMIT DEVICE ACK
  // **************************************************************


  // **************************************************************
  //               SCAN DEFINITIONS: MANUAL ENTRIES
  // **************************************************************


  x_p = 0;
  y_p = 0;
  grid_count = 0;




  // **************************************************************
  //                    END OF SCAN PARAMETERS
  // **************************************************************


  // **************************************************************
  //                   LISTEN PORT and DETERMINE Plate-Type
  //            SETUP respect to input: "P" + id
  //  id=0, 6 Well
  //  id=1, 12 Well...
  // **************************************************************

  String sub = "";
  char val;
  while (!started)
  {
    if (wait_byte() == 'P')
    {
      val = wait_byte();
      if (isDigit(val))
      {

        sub += (char)val;
        plate_type = sub.toInt();
        sub = "";
        Serial.print("O");
        started = true;
        beep(BEEP_pin, 50, 1);

      }
    }
  }
  reset2origin(0);
  reset2origin(1);
  digitalWrite(LEDPWM_pin, 1);
  delay(500);
  digitalWrite(LEDPWM_pin, 0);
  delay(500);
  digitalWrite(LEDPWM_pin, 1);
  delay(500);
  digitalWrite(LEDPWM_pin, 0);
  delay(500);
  digitalWrite(LEDPWM_pin, 1);

}

void loop() {

  int task;
  if (!finished) {

    task = decodeMove();
    if (task == 1) // 1: Goto "XnYm"
    {
      if (gotoWell())
      {
        Serial.print("O");
        grid_count++;
        delay(100);
      }
    }
    else if (task >= 2 && task < 6) // 2: Zp, Zn, xp, xn, yp, yn, zp, zn
    {
      Serial.print("O");
      delay(5);
      //;
      at_wellcenter = false;
    }
    else if (task == 6 && grid_selected == true) //capture grid
    {
      scanwell(plate_type);
    }
    else if (task == 7)
    {
      grid_selected = true;
      Serial.print("O");
    }
    else if (task == 8) // Light Control
    {
      Serial.print("O");
      delay(5);

    }
    else if (task == 0) //finish count
    {

      finished = true;
      digitalWrite(LEDPWM_pin, 0);


      delay(100);

      beep(BEEP_pin, 100, 2);
      reset2origin(0);
      reset2origin(1);
      asm volatile ("  jmp 0");
    }
    else if (task == -1)
    {
      Serial.print("U");
      delay(10);
    }

  }
  delay(50);
}

void stepXYZ(int motor_no, int yon, unsigned long adim, int kip)
{
  int Plength;

  // All OFF
  digitalWrite(Xen, HIGH);
  digitalWrite(Yen, HIGH);
  digitalWrite(Zen, HIGH);

  digitalWrite(XYZSlowMode, kip);// kip = 0 -> Full drive, kip = 1 -> 1/32 drive

  digitalWrite(XYZdir, yon); // dir = 0 ?
  digitalWrite(Xen + motor_no, LOW);//ON
  delay(1);
  if (motor_no == 2)
    Plength = 200;
  else if (kip == 0)
    Plength = PFullDrive;
  else
    Plength = PSlowDrive;

  int i;
  //Backlash Correction is applied if opposite direction is selected
  if (yon == intNOT( lastdir[motor_no]))
  {
    if (kip == 1)

    {
      digitalWrite(XYZdir, lastdir[motor_no]);// keep the last direction

      for (i = 0; i < 32; i++)
      {

        digitalWrite(XYZstep, HIGH);
        delayMicroseconds(Plength);
        digitalWrite(XYZstep, LOW);
        delayMicroseconds(Plength);
      }
      digitalWrite(XYZdir, yon);//
      delay(1);
      for (i = 0; i < (32 + lashes[motor_no]) ; i++)
      {

        digitalWrite(XYZstep, HIGH);
        delayMicroseconds(Plength);
        digitalWrite(XYZstep, LOW);
        delayMicroseconds(Plength);
      }

    }

  }

  lastdir[motor_no] = yon;


  for (i = 0; i < adim; i++)
  {

    digitalWrite(XYZstep, HIGH);
    delayMicroseconds(Plength);
    digitalWrite(XYZstep, LOW);
    delayMicroseconds(Plength);
  }
  if (yon == 0)
    Positions[motor_no] -= adim;
  else
    Positions[motor_no] += adim;
  digitalWrite(Xen + motor_no, HIGH);//OFF
  delay(1);
}
void beep(int pin, int duration, int number)
{
  int c;
  for (c = 0; c < number; c++)
  {
    digitalWrite(pin, HIGH);
    delay(duration);
    digitalWrite(pin, LOW);
    delay(duration);
  }
}
void reset2origin(int axis)
{
  unsigned long i;
  i = 0;
  short j;
  int Plength = SLOWMODE2;//PFullDrive;
  digitalWrite(XYZSlowMode, 1);// kip = 0 -> Full drive, kip = 1 -> 1/32 drive
  digitalWrite(XYZdir, 1); // dir = 0 ?
  digitalWrite(Xen + axis, LOW);//ON
  if (axis == 0)
  {
    while (digitalRead(SWITCH_x_pin))
    {
      digitalWrite(XYZstep, HIGH);
      delayMicroseconds(Plength);
      digitalWrite(XYZstep, LOW);
      delayMicroseconds(Plength);
      i++;
    }
  }
  else if (axis == 1)
  {
    while (digitalRead(SWITCH_y_pin))
    {
      digitalWrite(XYZstep, HIGH);
      delayMicroseconds(Plength);
      digitalWrite(XYZstep, LOW);
      delayMicroseconds(Plength);
      i++;
    }
  }

  digitalWrite(Xen + axis, HIGH);//OFF
  if (axis==0)
  {
  stepXYZ(axis, 1, 1600, 1);
  stepXYZ(axis, 0, 1600+resetsteps, 1);
  }
  else if (axis==1)
    {
  stepXYZ(axis, 1, 64, 1);
  stepXYZ(axis, 0, 64+resetsteps, 1);
  }
  
  
  beep(BEEP_pin, 20, 5);
  lastdir[axis] = 0;
}


int intNOT(int inp)
{
  if (inp > 0)
    return 0;
  else
    return 1;
}
char wait_byte()
{
  char inByte;
  do
  {
    delayMicroseconds(500);
  }
  while (Serial.available() == 0);
  inByte = Serial.read();
  return inByte;
}
char waitCommand()
{
  bool ok = false;
  char recbyte;

  do
  {
    recbyte = Serial.read();

    if ( recbyte == 'X' | recbyte == 'F' | recbyte == 'Z' | recbyte == 'x' | recbyte == 'y' | recbyte == 'z' | recbyte == 'G' | recbyte == 'r' | recbyte == 'i')
      ok = true;
  }
  while (!ok);
  return recbyte;//X, F, Z, x, y, z, G, r or i
}

int decodeMove()
{
  String sub = "";
  char x, option;
  option = waitCommand();
  if (option == 'X') {    // TASK 1: X-Y Movement
    //Serial.print("X");
    x = wait_byte();

    if (isDigit(x))
    {
      sub += (char)x;

      x_i = sub.toInt();
      sub = "";
    }

    if (wait_byte() == 'Y') { //may have two digits 1. or 12
      //Serial.print("Y");
      x = wait_byte();
      if (isDigit(x))
      {
        sub += (char)x;
        x = wait_byte();
        if (isDigit(x) && x > 0)
        {
          sub += (char)x;
          y_i = sub.toInt();
          sub = "";
        }
        else
        {
          y_i = sub.toInt();
          sub = "";
        }
      }
    }
    return 1;//X-Y Movement
  }
  else if (option == 'Z') // TASK 2: coarse focus
  {
    if (grid_count > 0)
    {
      x = wait_byte();
      beep(BEEP_pin, 20, 1);
      if (x == 'p')
      {
        stepXYZ(2, 1, zstep, 1);
        Zcurrent += zstep;
        return 2;
      }
      else if (x == 'n')
      {
        stepXYZ(2, 0, zstep, 1);
        Zcurrent -= zstep;
        return 2;
      }
    }
    else //grid_count = 0
      return -1;
  }
  else if (option == 'z') // TASK 5: fine focus
  {
    if (grid_count > 0)
    {
      x = wait_byte();
      beep(BEEP_pin, 20, 1);
      if (x == 'p') //Lens Up
      {
        stepXYZ(2, 1, zstep_fine, 1);
        Zcurrent += zstep_fine;
        return 5;
      }
      else if (x == 'n') // Lens Down
      {
        stepXYZ(2, 0, zstep_fine, 1);
        Zcurrent -= zstep_fine;
        return 5;
      }
    }
  }
  else if (option == 'x') // TASK 3: precise X
  {
    if (grid_count > 0)
    {
      x = wait_byte();
      beep(BEEP_pin, 50, 2);
      if (x == 'p')
      {
        stepXYZ(0, 1, xstep, 1);
        //Xcurrent += xstep;
        deltaX += xstep;
        return 3;
      }


      else if (x == 'n')
      {
        stepXYZ(0, 0, xstep, 1);
        //Xcurrent -= xstep;
        deltaX -= xstep;

        return 3;
      }
    }

    else //grid_count = 0
    {
      return -1;
    }
  }
  else if (option == 'y') // TASK 4: precise Y
  {
    if (grid_count > 0)
    {

      x = wait_byte();
      beep(BEEP_pin, 50, 2);
      if (x == 'p')
      {
        stepXYZ(1, 0, ystep, 1);
        //Ycurrent += ystep;
        deltaY += ystep;

        return 4;
      }
      else if (x == 'n')
      {
        stepXYZ(1, 1, ystep, 1);
        //Ycurrent -= ystep;
        deltaY -= ystep;

        return 4;
      }
    }

    else //grid_count = 0
      return -1;
  }

  else if (option == 'G')
  {
    return 6;
  }
  else if (option == 'r') { // TASK 7: Grid Selection

    x = wait_byte();
    sub = "";
    if (isDigit(x))
    {
      sub += (char)x;
      x = wait_byte();
      if (isDigit(x) && x > 0)
      {
        sub += (char)x;
        rows = sub.toInt();
        sub = "";
      }
      else
      {
        rows = sub.toInt();
        sub = "";
      }
    }


    if (wait_byte() == 'c') { //may have two digits 1. or 12
      //Serial.print("Y");
      x = wait_byte();
      if (isDigit(x))
      {
        sub += (char)x;
        x = wait_byte();
        if (isDigit(x) && x > 0)
        {
          sub += (char)x;
          cols = sub.toInt();
          sub = "";
        }
        else
        {
          cols = sub.toInt();
          sub = "";
        }
      }
    }

    return 7;
  }

  else if (option == 'i') // TASK 8 : LED CONTROL
  {
    if (grid_count > 0)
    {

      x = wait_byte();
      beep(BEEP_pin, 50, 2);
      if (x == 'p')
      {
        digitalWrite(LEDPWM_pin, 1);
        return 8;
      }
      else if (x == 'n')
      {

        digitalWrite(LEDPWM_pin, 0);
        return 8;
      }

    }

    else //grid_count = 0
      return -1;
  }
  else // "F"     // TASK 0: Finish/Reset
  {

    return 0;
  }
}

bool gotoWell() //requires global "count"
{

  unsigned long  x_move, y_move;
  x_move = 0;
  y_move = 0;
  unsigned long FastTrack;
  FastTrack = 1500; // 6 cm

  if (grid_count == 0)
  {

    stepXYZ(0, 0, X_offset[plate_type] / 32, 0);
    stepXYZ(1, 0, Y_offset[plate_type] / 32, 0);
    Xcurrent = 67200 - X_offset[plate_type];
    Ycurrent += Y_offset[plate_type];

    x_move = (x_i - 1) * aralik[plate_type];
    stepXYZ(0, 1, x_move / 32, 0);
    Xcurrent += x_move;

    y_move = (y_i - 1) * aralik[plate_type];
    delay(100);
    stepXYZ(1, 0, y_move / 32, 0);
    Ycurrent += y_move;

    x_p = x_i;
    y_p = y_i;
  }
  else
  {
    center_camera();

    if (y_i >= y_p) // move opposite to origin Y
    {
      y_move = (y_i - y_p) * aralik[plate_type];
      stepXYZ(1, 0, y_move / 32, 0);
      Ycurrent += y_move;
    }
    else // move towards origin Y
    {
      /* ******* 14.08.2018 *******
        // To prevent backlash, do not go towards -y, reset and then go towards +y.
        // The below line is cancelled
        // y_move = (y_p - y_i) * aralik[plate_type];
      */
      reset2origin(1);
      delay(2);
      y_move = (y_i - 1) * aralik[plate_type] + Y_offset[plate_type];
      stepXYZ(1, 0, y_move / 32, 0);
      Ycurrent += y_move ;
    }
    delay(100);
    if (x_i >= x_p)
    {
      x_move = (x_i - x_p) * aralik[plate_type];
      stepXYZ(0, 1, x_move / 32, 0);

      Xcurrent += x_move;
    }
    else
    {
      x_move = (x_p - x_i) * aralik[plate_type];
      stepXYZ(0, 0, x_move / 32, 0);
      Xcurrent -= x_move;
    }
    delay(100);
    x_p = x_i;
    y_p = y_i;
  }

  at_wellcenter = true;

  return true;
}

void center_camera()
{
  if (deltaX > 0)
    stepXYZ(0, 0, deltaX, 1);
  else if (deltaX < 0)
    stepXYZ(0, 1, -deltaX, 1);

  if (deltaY > 0)
    stepXYZ(1, 1, deltaY, 1);
  else if (deltaY < 0)
    stepXYZ(1, 0, -deltaY, 1);
  deltaX = 0;
  deltaY = 0;
}

void scanwell(int type)
{
  digitalWrite(LEDPWM_pin, 1);
  // Kamera ortada ola
  //FOVx = 156 adim
  //FOVy = 296 adim

  int Nx, Ny, dirx, diry, count_x, count_y;
  unsigned long dx, dy;
  unsigned long x_min, y_min;
  bool scanOK = false;
  int scanMode = 1;

  count_x = 1;
  count_y = 1;
  // goto top-left
  Nx = rows;//[type];// global arrays
  Ny = cols;//[type];

  dx = ((Nx - 1) / 2) * shiftX;
  dy = ((Ny - 1) / 2) * shiftY;

  //Reset X and Y
  reset2origin(0);
  delay(2);
  reset2origin(1);
  delay(2);
  //Go to MinY

  y_min = Y_offset[type] - dy + (y_i - 1) * aralik[type];
  PSlowDrive = SLOWMODE2;
  stepXYZ(1, 0, y_min , 1);
  PSlowDrive = SLOWMODE1;

  //Go to MinX

  x_min = X_offset[type] - (dx + ((x_i - 1) * aralik[type]));
  /*
                  PRINT VALUES FOR DEBUGGING!
  */

//  Serial.print("Xmin,Ymin: ");
//  Serial.print(x_min);
//  Serial.print(",");
//  Serial.println(y_min);
  // **************************

  PSlowDrive = SLOWMODE2;
  stepXYZ(0, 0, x_min , 1);
  PSlowDrive = SLOWMODE1;
  //stepXYZ(0, 0, (x_min - 1280) / 32 , 0); //Go to Minx, Fast
  //delay(2);
  //stepXYZ(0, 0, 1280 , 1); // Prior to target, slow down

  while (!scanOK)
  {
    if (count_x < Nx + 1)
    {
      if (count_x > 1)
      {
        stepXYZ(0, 0, shiftX, scanMode);
      }
      delay(100);
      Serial.print("C");
      do;
      while (!(wait_byte() == 'O'));
      count_x++;
      delay(100);
    }
    else
    {


      if (count_y < Ny )
      {
        // Type 3 Scan
        delay(50);
        reset2origin(1);
        delay(2);
        //Reset
        reset2origin(0);
        delay(2);
        PSlowDrive = SLOWMODE2;
  stepXYZ(1, 0, y_min + count_y * shiftY , 1);
  PSlowDrive = SLOWMODE1;
        delay(100);
        reset2origin(0);
        PSlowDrive = SLOWMODE2;
        stepXYZ(0, 0, x_min , 1);
        PSlowDrive = SLOWMODE1;
        //
        //stepXYZ(0, 0, (x_min - 1280) / 32 , 0); // minx
        //delay(2);
        //stepXYZ(0, 0, 1280 , 1); // minx

        count_x = 1;

        count_y++;
        delay(2);
      }
      else
      {

        scanOK = true;
        Serial.println("W");
        digitalWrite(LEDPWM_pin, 0);


      }
    }
  }
}






