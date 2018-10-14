/*
  Compatible with IncuStream Interface
  This version has seperate EN, DIR and STEP pins for X,Y and Z (Lens) axis motors.
  MODE is fixed to 1/32 microstepping via PCB. (MODE0,1,2 = Vcc)

  Developed by: Guray Gurkan, PhD
  e-mail: guray_gurkan@yahoo.co.uk
        : g.gurkan@iku.edu.tr
*/

// 14.08.2018: Scanning Types 1,2 and 3 introduced
//  Type 1: Snake Like Scanning
//  Type 2: Single Y axis Reset, X axis bouncing
//  Type 3: Single Y axis Reset, Xaxis reset and bouncing

// StepXYZ() function changed for Long type
// Plate Offset, spacing are now in Long type
// 1/32 Microstepping is used for every movement
// XmYn command relies on "grid_selected" flag

/******************************
      DRV8825 Enable,Direction, Step pins
******************************/

int EN[3] = {15, 5, 4};
int DIR[3] = {13, 7, 2}; // 1->Reset Yonu, 0-> Uzaklasma
int STEP[3] = {14, 6, 3};

int Positions[] = {0, 0, 0};

/*****************************
      More Externals
  X,Y Limit Switches,Buzzer, LED
*****************************/
int LEDPWM_pin = 8;
int BEEP_pin = 9;
int SWITCH_x_pin = 12;
int SWITCH_y_pin = 11 ;
int TRAY_pin = 10; // not used for current HW


/***********************************
  Well Plates vs. Steps required for TPP (1/32 Microstep)
***********************************/
//unsigned long spacing[5] = {30016, 19968, 15200, 6400, 7200};
unsigned long spacing[5] = {30016, 19968, 15104, 6400, 7200};
// In mm  {38, 25 , 19, ?, 9};

// A-1 Well Center Coordinates
//unsigned long X_offset[5] = {0, 0, 49872, 0, 53664};
unsigned long X_offset[5] = {0, 0, 49792, 0, 53664 - 256};
//unsigned long Y_offset[5] = {3200, 3200, 14800, 3200, 14304};
unsigned long Y_offset[5] = {3200, 3200, 14720, 3200, 14304};


/********************************
  SUB- GRID CAMERA SHIFT steps for non-overlap
    DO NOT MODIFY!
*********************************/
unsigned long shiftX = 312;//288;
unsigned long shiftY = 528;//536;



// SUB-GRID scan row and column numbers
int rows;
int cols;

// LIVE VIEW PARAMETERS
int zstep = 320;
int zstep_fine = 40;
int xstep = 100;
int ystep = 100;

int resetsteps = 896; // in microstepping (1/32) drive mode

int deltaX;
int deltaY;

unsigned long  Xcurrent = 0;
unsigned long Ycurrent = 0;
unsigned long Zcurrent = 0;


#define PULSE_YSTEP 15
#define PULSE_SLOW 80
#define PULSE_GRID 15 //100
#define PULSE_FAST 10
#define SLOWMODELENS 10



int PulseReset = 10;

/*
 *************************************
      BACKLASH CORRECTION
 *************************************
*/
byte lastdir[3] = {0, 0, 0}; // last directions
unsigned long lashes[3] = {0 , 0, 704};// 1/32 drive
int last_motor = 2;

boolean finished = false;
boolean started = false;
boolean grid_selected = false;

int plate_type;
int x_i, y_i;
int x_p, y_p;
int grid_count;
bool at_wellcenter = false;

void setup() {

  int i;
  for (i = 0; i < 3; i++)
  {
    pinMode(EN[i], OUTPUT);
    pinMode(DIR[i], OUTPUT);
    pinMode(STEP[i], OUTPUT);
  }
  pinMode(BEEP_pin, OUTPUT);
  pinMode(LEDPWM_pin, OUTPUT);
  pinMode(SWITCH_x_pin, INPUT_PULLUP);//0 if at limit
  pinMode(SWITCH_y_pin, INPUT_PULLUP);
  delay(100);
  pinMode(14, OUTPUT);
  pinMode(15, OUTPUT);
  digitalWrite(EN[0], HIGH);
  digitalWrite(EN[1], HIGH);
  digitalWrite(EN[2], HIGH);

  delay(100);
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

  x_i = 0;
  y_i = 0;
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
  //  id=1, 12 Well
  //  id=2, 24 Well
  //  id=3, 48 Well
  //  id=4, 96 Well
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

  checkPower();
  delay(1000);
  digitalWrite(EN[0], LOW);
  reset2origin(0);
  digitalWrite(EN[0], HIGH);
  delay(10);

  digitalWrite(EN[1], LOW);
  reset2origin(1);
  digitalWrite(EN[1], HIGH);

  digitalWrite(LEDPWM_pin, 1);
  delay(1000);
  digitalWrite(LEDPWM_pin, 0);
  delay(1000);
  digitalWrite(LEDPWM_pin, 1);
}

void loop() {

  int task;
  if (!finished) {

    task = decodeMove();
    if (task == 1 && grid_selected == false) // 1: Goto "XnYm"
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
      grid_count++;
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
      digitalWrite(EN[0], LOW);
      reset2origin(0);
      digitalWrite(EN[0], HIGH);
      digitalWrite(EN[1], LOW);
      reset2origin(1);
      digitalWrite(EN[1], HIGH);
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

void stepXYZ(int axis, int dir, unsigned long steps, int pulse)
{
  if (checkPower()) // If external power connected!
  {
    unsigned long i;

    if (axis == 2) // Z?
    {
      digitalWrite(EN[axis] , LOW);
      last_motor = 2;
    }

    digitalWrite(DIR[axis], dir); // dir = 0 ?


    //Backlash Correction is applied to Z Axis if opposite direction is selected
    if (dir == intNOT( lastdir[axis]) & axis == 2)
    {
      digitalWrite(DIR[axis], intNOT(dir));


      for (i = 0; i < 32; i++)
      {

        digitalWrite(STEP[axis], HIGH);
        delayMicroseconds(pulse);
        digitalWrite(STEP[axis], LOW);
        delayMicroseconds(pulse);
      }
      digitalWrite(DIR[axis], dir);//
      delay(1);
      for (i = 0; i < (32 + lashes[axis]) ; i++)
      {

        digitalWrite(STEP[axis], HIGH);
        delayMicroseconds(pulse);
        digitalWrite(STEP[axis], LOW);
        delayMicroseconds(pulse);
      }
    }

    lastdir[axis] = dir;

    for (i = 0; i < steps; i++)
    {

      digitalWrite(STEP[axis], HIGH);
      delayMicroseconds(pulse);
      digitalWrite(STEP[axis], LOW);
      delayMicroseconds(pulse);
    }

    if (axis == 2)
      digitalWrite(EN[axis], HIGH);//OFF if Z.
    delay(1);
  }
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
  if (checkPower())
  {
    if (axis < 2)
    {
      unsigned long i;
      i = 0;
      short j;
      digitalWrite(DIR[axis], 1);

      if (axis == 0)
      {

        while (digitalRead(SWITCH_x_pin))
        {
          digitalWrite(STEP[axis], HIGH);
          delayMicroseconds(PulseReset);
          digitalWrite(STEP[axis], LOW);
          delayMicroseconds(PulseReset);
          i++;
        }
      }
      else if (axis == 1)
      {
        while (digitalRead(SWITCH_y_pin))
        {
          digitalWrite(STEP[axis], HIGH);
          delayMicroseconds(PulseReset);
          digitalWrite(STEP[axis], LOW);
          delayMicroseconds(PulseReset);
          i++;
        }
      }

      if (axis == 0)
      {
        stepXYZ(axis, 1, 1280, PULSE_SLOW);
        stepXYZ(axis, 0, 1280 + resetsteps, PULSE_SLOW);
      }
      else if (axis == 1)
      {
        stepXYZ(axis, 1, 128, PULSE_SLOW);
        stepXYZ(axis, 0, 128 + resetsteps, PULSE_SLOW);
      }


      beep(BEEP_pin, 20, 5);
      lastdir[axis] = 0;


    }
  }
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
        stepXYZ(2, 1, zstep, SLOWMODELENS);
        Zcurrent += zstep;
        return 2;
      }
      else if (x == 'n')
      {

        stepXYZ(2, 0, zstep, SLOWMODELENS);
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
        stepXYZ(2, 1, zstep_fine, SLOWMODELENS);
        Zcurrent += zstep_fine;
        return 5;
      }
      else if (x == 'n') // Lens Down
      {
        stepXYZ(2, 0, zstep_fine, SLOWMODELENS);
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
        digitalWrite(EN[0], LOW);
        stepXYZ(0, 1, xstep, PULSE_SLOW);
        //Xcurrent += xstep;
        deltaX += xstep;
        digitalWrite(EN[0], HIGH);
        return 3;
      }


      else if (x == 'n')
      {
        digitalWrite(EN[0], LOW);
        stepXYZ(0, 0, xstep, PULSE_SLOW);
        //Xcurrent -= xstep;
        deltaX -= xstep;
        digitalWrite(EN[0], HIGH);
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
        digitalWrite(EN[1], LOW);
        stepXYZ(1, 0, ystep, PULSE_SLOW);
        //Ycurrent += ystep;
        deltaY += ystep;
        digitalWrite(EN[1], HIGH);
        return 4;
      }
      else if (x == 'n')
      {
        digitalWrite(EN[1], LOW);
        stepXYZ(1, 1, ystep, PULSE_SLOW);
        //Ycurrent -= ystep;
        deltaY -= ystep;
        digitalWrite(EN[1], HIGH);
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
  //Enable Motors
  digitalWrite(EN[0], LOW);
  delay(250);
  digitalWrite(EN[1], LOW);

  if (grid_count == 0)
  {

    stepXYZ(0, 0, X_offset[plate_type] , PULSE_FAST);
    stepXYZ(1, 0, Y_offset[plate_type], PULSE_FAST);
    //Xcurrent = 67200 - X_offset[plate_type];
    //Ycurrent += Y_offset[plate_type];

    x_move = (x_i - 1) * spacing[plate_type];
    stepXYZ(0, 1, x_move, PULSE_FAST);
    //Xcurrent += x_move;

    y_move = (y_i - 1) * spacing[plate_type];
    delay(1);
    stepXYZ(1, 0, y_move, PULSE_FAST);
    //Ycurrent += y_move;

    x_p = x_i;
    y_p = y_i;
  }
  else
  {
    center_camera();

    if (y_i >= y_p) // move opposite to origin Y
    {
      y_move = (y_i - y_p) * spacing[plate_type];
      stepXYZ(1, 0, y_move, PULSE_FAST);
      //Ycurrent += y_move;
    }
    else // move towards origin Y
    {
      /* ******* 14.08.2018 *******
        // To prevent backlash, do not go towards -y, reset and then go towards +y.
        // The below line is cancelled
        // y_move = (y_p - y_i) * spacing[plate_type];
      */
      reset2origin(1);
      delay(2);
      y_move = (y_i - 1) * spacing[plate_type] + Y_offset[plate_type];
      stepXYZ(1, 0, y_move, PULSE_FAST);
      //Ycurrent += y_move ;
    }
    delay(1);
    if (x_i >= x_p)
    {
      x_move = (x_i - x_p) * spacing[plate_type];
      stepXYZ(0, 1, x_move, PULSE_FAST);

      //Xcurrent += x_move;
    }
    else
    {
      x_move = (x_p - x_i) * spacing[plate_type];
      stepXYZ(0, 0, x_move, PULSE_FAST);
      //Xcurrent -= x_move;
    }
    delay(1);
    x_p = x_i;
    y_p = y_i;
  }

  at_wellcenter = true;
  delay(100);
  // MOTORS OFF
  digitalWrite(EN[0], HIGH);
  delay(100);
  digitalWrite(EN[1], HIGH);
  delay(100);
  return true;
}

void center_camera()
{
  if (deltaX > 0)
    stepXYZ(0, 0, deltaX, PULSE_SLOW);
  else if (deltaX < 0)
    stepXYZ(0, 1, -deltaX, PULSE_SLOW);

  if (deltaY > 0)
    stepXYZ(1, 1, deltaY, PULSE_SLOW);
  else if (deltaY < 0)
    stepXYZ(1, 0, -deltaY, PULSE_SLOW);
  deltaX = 0;
  deltaY = 0;
}

void scanwell(int type)
{

  unsigned long dx, dy, Nx, Ny, dirx, diry, count_x, count_y;
  unsigned long x_min, y_min;
  bool scanOK = false;
  int xdir = 0;// go up first

  count_x = 1;
  count_y = 1;

  // goto top-left
  Nx = rows;//[type];// global arrays
  Ny = cols;//[type];

  dx = ((Nx - 1) / 2) * shiftX;
  dy = ((Ny - 1) / 2) * shiftY;

  digitalWrite(EN[0], LOW);
  delay(50);
  digitalWrite(EN[1], LOW);
  delay(50);

  //Reset X and Y
  reset2origin(0);
  delay(2);
  reset2origin(1);
  delay(2);
  //Go to MinY

  y_min = Y_offset[type] + (y_i - 1) * spacing[type];
  y_min = y_min - dy;

  stepXYZ(1, 0, y_min-500 , PULSE_FAST);
  delay(10);
  stepXYZ(1, 0, 500 , PULSE_GRID);
    
  //Go to MinX
  x_min = X_offset[type] - (dx + ((x_i - 1) * spacing[type]));
  stepXYZ(0, 0, x_min-500 , PULSE_FAST);
  stepXYZ(0, 0, 500 , PULSE_GRID);

  // Reset Stepper Indices
 
delay(100);

  while (!scanOK)
  {
    digitalWrite(LEDPWM_pin, 1);
    delay(10);
    if (count_x < Nx + 1)
    {

      if (count_x > 1)
      {
        if (count_x == 2 & count_y>1)//except for first column and first rows
        {
          stepXYZ(0, xdir, lashes[0], PULSE_GRID);
        }
        stepXYZ(0, xdir, shiftX, PULSE_GRID);
      }
      delay(100);
      Serial.print("C");
      delay(10);
      do;
      while (!(wait_byte() == 'O'));
      count_x++;
      delay(50);
    }
    else
    {


      if (count_y < Ny )
      {
        // Type 1 Scan
        //digitalWrite(EN[1], LOW);
        stepXYZ(1, 0, shiftY , PULSE_YSTEP);
        delay(10);
        //digitalWrite(EN[1], HIGH);
        //Go to minx
        //reset2origin(0);//?
        //delay(10);
        //stepXYZ(0, 0, x_min , SLOWMODE2);

        // stepXYZ(0, 1, shiftX*(Nx-1) , SLOWMODE1);

        xdir = intNOT(xdir);//toggle xdirection if rows finished

        delay(1);
        count_x = 1;
        count_y++;
        delay(1);
      }
      else
      {

        scanOK = true;
        Serial.println("W");
        digitalWrite(LEDPWM_pin, 0);
        digitalWrite(EN[0], HIGH);
        digitalWrite(EN[1], HIGH);
      }
    }
  }
}

bool checkPower()
{
  while (analogRead(A5) > 50)
  {
    beep(BEEP_pin, 50, 2);
  }
  return true;
}





