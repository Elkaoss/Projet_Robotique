/*
 * ROBOT RADAR AUTONOME mBOT - VERSION FINALE
 * Compatible avec l'interface Python Tkinter
 * * Fonctionnement :
 * 1. Appui bouton carte : Démarrage / Arrêt.
 * 2. Avance tout droit.
 * 3. Si obstacle < 50cm OU Si temps écoulé > 4 sec (1 mètre) :
 * -> ARRET
 * -> SCAN 360° (Envoi des données A:angle,D:distance)
 * -> Reprise de la route.
 */

#include <MeMCore.h>
#include <Wire.h>
#include <SoftwareSerial.h>

// --- CONFIGURATION MATÉRIELLE ---
MeUltrasonic ultr(PORT_3);   // Capteur Ultrason branché sur le Port 3
MeDCMotor MotorL(M1);        // Moteur Gauche
MeDCMotor MotorR(M2);        // Moteur Droit
MeRGBLed rgb(7, 2);          // LED intégrée
MeBuzzer buzzer;             // Buzzer intégré

// --- PARAMÈTRES (À CALIBRER SELON LA BATTERIE) ---
int moveSpeed = 160;         // Vitesse pour avancer
int turnSpeed = 140;         // Vitesse pour tourner (scan)

// Temps estimé pour parcourir 1 mètre (en millisecondes)
const unsigned long TIME_FOR_1M = 4000; 

// Temps estimé pour faire un tour complet 360° (en millisecondes)
const unsigned long TIME_FOR_360 = 2300; 

// --- VARIABLES GLOBALES ---
bool isRunning = false;          // État du robot
unsigned long lastMoveTime = 0;  // Chronomètre pour la distance
float dist = 0;                  // Distance lue

void setup() {
  // Initialisation Série à 115200 bauds (Doit correspondre au Python)
  Serial.begin(115200); 
  
  // Initialisation LED
  rgb.setNumber(16);
  rgb.setColor(0, 0, 50); // Bleu (En attente)
  rgb.show();
  
  Stop(); // Moteurs à l'arrêt au démarrage
}

void loop() {
  // ---------------------------------------------------------
  // 1. GESTION DU BOUTON (ON/OFF)
  // ---------------------------------------------------------
  // Le bouton du mCore est sur A7.
  if (analogRead(A7) < 10) { 
    delay(50); // Anti-rebond
    if (analogRead(A7) < 10) {
      isRunning = !isRunning; // On inverse l'état
      
      if (isRunning) {
        // Démarrage
        rgb.setColor(0, 50, 0); // Vert = Marche
        rgb.show();
        buzzer.tone(1000, 200);
        lastMoveTime = millis(); // Reset du chrono distance
        delay(1000); // Pause pour retirer le doigt
      } else {
        // Arrêt
        Stop();
        rgb.setColor(50, 0, 0); // Rouge = Stop
        rgb.show();
        buzzer.tone(500, 200);
        while(analogRead(A7) < 10); // Attendre relâchement du bouton
      }
    }
  }

  // Si le robot est en pause, on arrête la boucle ici
  if (!isRunning) return;

  // ---------------------------------------------------------
  // 2. LECTURE CAPTEURS & LOGIQUE
  // ---------------------------------------------------------
  dist = ultr.distanceCm();
  if (dist == 0) dist = 400; // Filtrage des erreurs (0 = infini ou erreur)

  // CAS A : OBSTACLE PROCHE (< 50cm)
  if (dist < 50) {
    Stop();
    buzzer.tone(2000, 100); // Bip d'alerte
    delay(200);
    
    // Lancer le scan radar
    performRadarScan();
    
    // Manœuvre d'évitement après le scan
    Backward(); delay(500);
    TurnLeft(); delay(600); // Tourne d'environ 90° (ajuster le délai si besoin)
    Stop();     delay(200);
    
    lastMoveTime = millis(); // Reset du chrono 1m
  }
  
  // CAS B : 1 MÈTRE PARCOURU (Simulé par le temps)
  else if (millis() - lastMoveTime > TIME_FOR_1M) {
    Stop();
    buzzer.tone(1000, 100); 
    delay(100);
    
    // Lancer le scan radar automatique
    performRadarScan();
    
    lastMoveTime = millis(); // Reset du chrono 1m
  }
  
  // CAS C : VOIE LIBRE -> AVANCER
  else {
    Forward();
  }
}

// ---------------------------------------------------------
// FONCTION DE SCAN RADAR (FORMAT COMPATIBLE PYTHON)
// ---------------------------------------------------------
void performRadarScan() {
  rgb.setColor(30, 0, 30); // Violet pendant le scan
  rgb.show();

  unsigned long scanStart = millis();
  
  // Rotation sur place (gauche)
  MotorL.run(-turnSpeed);
  MotorR.run(turnSpeed);

  // Boucle de scan pendant la durée d'un tour complet
  while (millis() - scanStart < TIME_FOR_360) {
    
    // 1. Calcul de l'angle (règle de trois temporelle)
    float progress = (float)(millis() - scanStart) / TIME_FOR_360;
    int angle = progress * 360;
    
    // 2. Lecture distance
    float reading = ultr.distanceCm();
    if(reading == 0) reading = 400; 

    // 3. ENVOI DES DONNEES (Format: "A:angle,D:distance")
    // C'est ce format précis que votre script Python attend.
    Serial.print("A:");
    Serial.print(angle);
    Serial.print(",D:");
    Serial.println((int)reading);
    
    delay(40); // Petite pause pour stabiliser le capteur
  }

  Stop(); // Fin du tour
  
  rgb.setColor(0, 50, 0); // Retour au vert
  rgb.show();
  delay(500); // Stabilisation avant de repartir
}

// ---------------------------------------------------------
// FONCTIONS DE MOUVEMENT BASIQUES
// ---------------------------------------------------------
void Forward() {
  MotorL.run(-moveSpeed);
  MotorR.run(moveSpeed);
}

void Backward() {
  MotorL.run(moveSpeed);
  MotorR.run(-moveSpeed);
}

void TurnLeft() {
  MotorL.run(-moveSpeed); // Les deux moteurs dans le même sens pour tourner sur place
  MotorR.run(-moveSpeed);
}

void Stop() {
  MotorL.run(0);
  MotorR.run(0);
}