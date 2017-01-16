#include "Targeter.h"
#include "Arduino.h"
#include "plane.h"

Targeter::Targeter() {
  // Empty constructor
}

// ------------------------------------ PHYSICAL CALCULATIONS ------------------------------------

/*
     Step 1
     Computes shortest distance from the line defined by the plane's path
     to the location of the target.
*/

double Targeter::calculateLateralError() {
  // See "Line defined by two points" at
  // https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
  // From the wikipedia equation:
  // P1 => prevPos
  // P2 => curPos
  // (x0, y0) => targetPos

  double currentEasting, currentNorthing;
  double targetEasting, targetNorthing;

  // Get current coordinates
  convertDeg2UTM(convertDecimalDegMinToDegree(currentLatitude), convertDecimalDegMinToDegree(currentLongitude), currentEasting, currentNorthing);
  // Get target coordinates
  convertDeg2UTM(convertDecimalDegMinToDegree(targetLatitude), convertDecimalDegMinToDegree(targetLongitude), targetEasting, targetNorthing);

  // P1:
  // Generate an arbitrary point to define the line (far enough away to avoid rounding errors)
  double currentHeadingMathAngle = convertHeadingToMathAngle(currentHeading);
  double x1 = currentEasting + cos(currentHeadingMathAngle / 180 * PI) * 2000;
  double y1 = currentNorthing + sin(currentHeadingMathAngle / 180 * PI) * 2000;

  // P2:
  double x2 = currentEasting;
  double y2 = currentNorthing;

  // Target
  double x0 = targetEasting;
  double y0 = targetNorthing;

  // Calculate numerator in equation: (it's a big equation...)
  double numerator = (y2 - y1) * x0;
  numerator -= (x2 - x1) * y0;
  numerator += x2 * y1;
  numerator -= y2 * x1;
  numerator = abs(numerator);

  // Calculate denominator in equation:
  double denominator = pow(y2 - y1, 2.0);
  denominator += pow(x2 - x1, 2.0);
  denominator = sqrt(denominator);

  return numerator / denominator;
}

/*
   Step 2
   Calculates direct distance from current position to the target.
*/

double Targeter::calculateDirectDistanceToTarget() {

  double currentEasting, currentNorthing;
  double targetEasting, targetNorthing;

  // Get current coordinates
  convertDeg2UTM(convertDecimalDegMinToDegree(currentLatitude), convertDecimalDegMinToDegree(currentLongitude), currentEasting, currentNorthing);
  // Get target coordinates
  convertDeg2UTM(convertDecimalDegMinToDegree(targetLatitude), convertDecimalDegMinToDegree(targetLongitude), targetEasting, targetNorthing);

  double dist = 0;
  // Use pythagorean theorem to calculate distance between curPos and targetPos:
  dist = sqrt(pow((targetEasting - currentEasting), 2) + pow((targetNorthing - currentNorthing), 2));
  return dist;
}


/*
   Step 3
   Calculates distance along current path until nearest point to target.
   Note: Relies on the fact that an accurate lateral error has already been calculated
*/

double Targeter::calculatePathDistanceToTarget() {
  double dist = 0;
  dist = sqrt(pow(calculateDirectDistanceToTarget(), 2) - pow(lateralError, 2));
  //System.out.print("Distance to target along path: ");
  //System.out.println(dist);
  return dist;
}


/*
   Step 4
   Calculates how early (distance in m before being above the target) the
   package must be dropped based on current speed and altitude.

   *** Note: right now this is ignores air resistance and is very unrealistic.
   Some thoughts on air resistance: terminal falling velocity 35-45 m/s ish, which would be reached at 4.1s or 82m / 270ft
   It will accelerate slower once above low speeds
   @ 10 m/s estimated air resistance acceleration would be -0.6 m/s^2. If fall time on the order of 3s, this would
   only affect speed vi = 10, vf = 8.2, vavg = 9.1.  So not a lot, and is partially offset by reduced fall speed
   Approximate Equations: m = 2kg, cd =1 CSA = 0.02m^2, rho = 1.25   a = 0.5*cd*rho*CSA*v^2/m = 0.00625*v^2
   Overall approximation of 0.9 correction factor
   See MATLAB script for more details
*/
const double CORRECTION_FACTOR = 0.9;

double Targeter::calculateDropDistance() {
  double fallTime = 0;
  double horizDist = 0;
  // targetPos.getAltitude() will probably be 0, but I included it just in case:
  //double heightm = (curPos.getAltitude() - targetPos.getAltitude())  / 3.28084; // convert feet to meters
  double heightm = (currentAltitude - targetAltitude); // Altitudes are already in m
  // Calculate time for payload to fall:
  if (heightm < 0) {
    heightm = 0;  //prevent NaN from sqrt
  }

  fallTime = sqrt(2 * heightm / 9.807); // time in seconds

  // Calculate horizontal distance that payload will travel in this time:
  horizDist = currentVelocity * fallTime * CORRECTION_FACTOR;
  return horizDist;
}


/*
   Step 5
   Calculates time until optimal drop location.
*/

double Targeter::calculateTimeToDrop() {
  double time;
  double distRemaining = dropDistanceToTarget();
  // d = do - v*t
  // t = d/v
  time = ((distRemaining  - currentVelocity * ((millis() - currentDataAge) / 1000)) / currentVelocity);
  return time;
}

double Targeter::dropDistanceToTarget() {
  return calculatePathDistanceToTarget() - calculateDropDistance();
}

// ------------------------------------ GETTERS ------------------------------------

boolean Targeter::recalculate() {
  calculateLateralError();  
  calculateTimeToDrop();
  return dropDistanceToTarget() < targetRaduis;
}

double Targeter::getLateralError() {
  return lateralError;
}

double Targeter::getTimeToDrop() {
  return timeToDrop;
}

// ------------------------------------ SETTERS ------------------------------------

boolean Targeter::setCurrentData(double _currentLatitude, double _currentLongitude, double _currentAltitude, double _currentVelocity, double _currentHeading, double _currentDataAge) {

  currentLatitude = _currentLatitude;
  currentLongitude = _currentLongitude;

  currentAltitude = _currentAltitude;
  currentVelocity = _currentVelocity;
  currentHeading = _currentHeading;

  currentDataAge = _currentDataAge;

  calculateLateralError();  
  calculateTimeToDrop();

  return dropDistanceToTarget() < targetRaduis;

}

void Targeter::setTargetData(double _targetLatitude, double _targetLongitude, double _targetAltitude) {

  targetLatitude = _targetLatitude;
  targetLongitude = _targetLongitude;

  targetAltitude = _targetAltitude;

  calculateLateralError();  
  calculateTimeToDrop();

}

// ------------------------------------ CONVERT HEADING TO ANGLE ------------------------------------

double Targeter::convertHeadingToMathAngle(double heading) {
  // Transform from compass degrees to typical math degrees
  double angle = -1 * (heading - 90);
  if (angle < 0) {
    angle = 360 + angle;
  }
  return angle;
}

// ------------------------------------ CONVERT LAT/LON DEGREES TO UTM COORDINATES ------------------------------------

double Targeter::convertDecimalDegMinToDegree(double decimalDegreeMin) {  //accepts format AAAYY.ZZZZZ  AAA = degrees,, YY = minutes,  ZZZZZZ = decimal minutes
  // Assumes the N/S & E/W sign convention is followed (N = +ve, W = -ve).
  // This is currently done in MainWindow in analyzePacket

  int baseDegree = (int)(decimalDegreeMin / 100.0);   // Extracts AAA (the cast will remove the shifted digits /100.0 ->  AAA.YYZZZZZ, cast -> AAA)
  double baseDegMins = decimalDegreeMin - 100 * baseDegree;  // AAAYY.ZZZ - AAA*100 = AAAYY.ZZ - AAA00.0 = YY.ZZZZ
  return baseDegree + baseDegMins / 60.0; // 1 degree minute = 1/60 degrees
}

void Targeter::convertDeg2UTM(double lat, double lon, double &utmEasting, double &utmNorthing) {

  char utmLetter;
  int utmZone = (int) floor(lon / 6 + 31);

  if (lat < -72)
    utmLetter = 'C';
  else if (lat < -64)
    utmLetter = 'D';
  else if (lat < -56)
    utmLetter = 'E';
  else if (lat < -48)
    utmLetter = 'F';
  else if (lat < -40)
    utmLetter = 'G';
  else if (lat < -32)
    utmLetter = 'H';
  else if (lat < -24)
    utmLetter = 'J';
  else if (lat < -16)
    utmLetter = 'K';
  else if (lat < -8)
    utmLetter = 'L';
  else if (lat < 0)
    utmLetter = 'M';
  else if (lat < 8)
    utmLetter = 'N';
  else if (lat < 16)
    utmLetter = 'P';
  else if (lat < 24)
    utmLetter = 'Q';
  else if (lat < 32)
    utmLetter = 'R';
  else if (lat < 40)
    utmLetter = 'S';
  else if (lat < 48)
    utmLetter = 'T';
  else if (lat < 56)
    utmLetter = 'U';
  else if (lat < 64)
    utmLetter = 'V';
  else if (lat < 72)
    utmLetter = 'W';
  else
    utmLetter = 'X';

  utmEasting = 0.5 * log((1 + cos(lat * PI / 180) * sin(lon * PI / 180 - (6 * utmZone - 183) * PI / 180)) / (1 - cos(lat * PI / 180) * sin(lon * PI / 180 - (6 * utmZone - 183) * PI / 180))) * 0.9996 * 6399593.62 / pow((1 + pow(0.0820944379, 2) * pow(cos(lat * PI / 180), 2)), 0.5) * (1 + pow(0.0820944379, 2) / 2 * pow((0.5 * log((1 + cos(lat * PI / 180) * sin(lon * PI / 180 - (6 * utmZone - 183) * PI / 180)) / (1 - cos(lat * PI / 180) * sin(lon * PI / 180 - (6 * utmZone - 183) * PI / 180)))), 2) * pow(cos(lat * PI / 180), 2) / 3) + 500000;
  utmEasting = round(utmEasting * 100) * 0.01;
  utmNorthing = (atan(tan(lat * PI / 180) / cos((lon * PI / 180 - (6 * utmZone - 183) * PI / 180))) - lat * PI / 180) * 0.9996 * 6399593.625 / sqrt(1 + 0.006739496742 * pow(cos(lat * PI / 180), 2)) * (1 + 0.006739496742 / 2 * pow(0.5 * log((1 + cos(lat * PI / 180) * sin((lon * PI / 180 - (6 * utmZone - 183) * PI / 180))) / (1 - cos(lat * PI / 180) * sin((lon * PI / 180 - (6 * utmZone - 183) * PI / 180)))), 2) * pow(cos(lat * PI / 180), 2)) + 0.9996 * 6399593.625 * (lat * PI / 180 - 0.005054622556 * (lat * PI / 180 + sin(2 * lat * PI / 180) / 2) + 4.258201531e-05 * (3 * (lat * PI / 180 + sin(2 * lat * PI / 180) / 2) + sin(2 * lat * PI / 180) * pow(cos(lat * PI / 180), 2)) / 4 - 1.674057895e-07 * (5 * (3 * (lat * PI / 180 + sin(2 * lat * PI / 180) / 2) + sin(2 * lat * PI / 180) * pow(cos(lat * PI / 180), 2)) / 4 + sin(2 * lat * PI / 180) * pow(cos(lat * PI / 180), 2) * pow(cos(lat * PI / 180), 2)) / 3);

  if (utmLetter < 'M') {
    utmNorthing = utmNorthing + 10000000;
  }

  utmNorthing = round(utmNorthing * 100) * 0.01;
}
