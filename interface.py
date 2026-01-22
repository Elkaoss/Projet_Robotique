import tkinter as tk
from tkinter import ttk
import math
import serial
import serial.tools.list_ports
import threading
import time

class RadarInterface:
    def __init__(self, root):
        self.root = root
        self.root.title("mBot Radar & Cartographie")
        self.root.geometry("1400x800")
        self.root.configure(bg='#000000')
        
        # Variables
        self.serial_port = None
        self.is_running = False
        self.current_angle = 0
        self.current_distance = 0
        self.radar_points = []  # Pour l'effet de traînée
        self.map_points = []  # Pour la cartographie persistante
        self.max_distance = 200  # Distance max en cm
        
        # Configuration interface
        self.setup_ui()
        
        # Démarrage animation
        self.animate_radar()
        
    def setup_ui(self):
        # Frame principal
        main_frame = tk.Frame(self.root, bg='#000000')
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Frame pour les contrôles en haut
        control_frame = tk.Frame(main_frame, bg='#000000')
        control_frame.pack(side=tk.TOP, fill=tk.X, pady=(0, 10))
        
        # Sélection du port série
        tk.Label(control_frame, text="Port Série:", bg='#000000', fg='#00ff00', 
                font=('Courier', 10)).pack(side=tk.LEFT, padx=5)
        
        self.port_var = tk.StringVar()
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo = ttk.Combobox(control_frame, textvariable=self.port_var, 
                                       values=ports, width=15, state='readonly')
        if ports:
            self.port_combo.current(0)
        self.port_combo.pack(side=tk.LEFT, padx=5)
        
        # Bouton refresh ports
        self.refresh_btn = tk.Button(control_frame, text="🔄", command=self.refresh_ports,
                                     bg='#001a00', fg='#00ff00', font=('Courier', 10),
                                     relief=tk.FLAT, padx=10)
        self.refresh_btn.pack(side=tk.LEFT, padx=5)
        
        # Bouton connexion
        self.connect_btn = tk.Button(control_frame, text="Connecter", 
                                     command=self.toggle_connection,
                                     bg='#003300', fg='#00ff00', font=('Courier', 10, 'bold'),
                                     relief=tk.FLAT, padx=15, pady=5)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        
        # Indicateur de statut
        self.status_label = tk.Label(control_frame, text="● Déconnecté", 
                                     bg='#000000', fg='#ff0000', font=('Courier', 10, 'bold'))
        self.status_label.pack(side=tk.LEFT, padx=20)
        
        # Bouton reset cartographie
        self.reset_btn = tk.Button(control_frame, text="Reset Carte", 
                                   command=self.reset_map,
                                   bg='#330000', fg='#ff0000', font=('Courier', 10),
                                   relief=tk.FLAT, padx=10)
        self.reset_btn.pack(side=tk.RIGHT, padx=5)
        
        # Frame pour les canvas
        canvas_frame = tk.Frame(main_frame, bg='#000000')
        canvas_frame.pack(fill=tk.BOTH, expand=True)
        
        # Frame Radar (gauche)
        radar_frame = tk.Frame(canvas_frame, bg='#000000', relief=tk.RAISED, bd=2)
        radar_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 5))
        
        tk.Label(radar_frame, text="RADAR SONAR", bg='#000000', fg='#00ff00',
                font=('Courier', 14, 'bold')).pack(pady=5)
        
        self.radar_canvas = tk.Canvas(radar_frame, bg='#000a00', 
                                     highlightthickness=2, highlightbackground='#00ff00')
        self.radar_canvas.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Info radar
        self.radar_info = tk.Label(radar_frame, text="Angle: 0° | Distance: 0 cm", 
                                  bg='#000000', fg='#00ff00', font=('Courier', 10))
        self.radar_info.pack(pady=5)
        
        # Frame Cartographie (droite)
        map_frame = tk.Frame(canvas_frame, bg='#000000', relief=tk.RAISED, bd=2)
        map_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(5, 0))
        
        tk.Label(map_frame, text="CARTOGRAPHIE 2D", bg='#000000', fg='#00ff00',
                font=('Courier', 14, 'bold')).pack(pady=5)
        
        self.map_canvas = tk.Canvas(map_frame, bg='#000a00',
                                   highlightthickness=2, highlightbackground='#00ff00')
        self.map_canvas.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Info cartographie
        self.map_info = tk.Label(map_frame, text="Points détectés: 0", 
                                bg='#000000', fg='#00ff00', font=('Courier', 10))
        self.map_info.pack(pady=5)
        
        # Dessiner les grilles initiales
        self.root.after(100, self.draw_radar_grid)
        self.root.after(100, self.draw_map_grid)
    
    def refresh_ports(self):
        """Rafraîchir la liste des ports série"""
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports and not self.port_var.get():
            self.port_combo.current(0)
    
    def toggle_connection(self):
        """Connecter/déconnecter au port série"""
        if not self.is_running:
            try:
                port = self.port_var.get()
                if not port:
                    return
                
                self.serial_port = serial.Serial(port, 9600, timeout=1)
                self.is_running = True
                self.connect_btn.config(text="Déconnecter", bg='#330000', fg='#ff0000')
                self.status_label.config(text="● Connecté", fg='#00ff00')
                
                # Démarrer le thread de lecture
                self.read_thread = threading.Thread(target=self.read_serial, daemon=True)
                self.read_thread.start()
            except Exception as e:
                self.status_label.config(text=f"● Erreur: {str(e)}", fg='#ff0000')
        else:
            self.is_running = False
            if self.serial_port:
                self.serial_port.close()
            self.connect_btn.config(text="Connecter", bg='#003300', fg='#00ff00')
            self.status_label.config(text="● Déconnecté", fg='#ff0000')
    
    def read_serial(self):
        """Lire les données du port série"""
        while self.is_running:
            try:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode('utf-8').strip()
                    # Format attendu: "A:120,D:85" (Angle:120°, Distance:85cm)
                    if 'A:' in line and 'D:' in line:
                        parts = line.split(',')
                        angle = int(parts[0].split(':')[1])
                        distance = int(parts[1].split(':')[1])
                        self.current_angle = angle
                        self.current_distance = distance
                        
                        # Ajouter le point pour la cartographie
                        if distance > 0 and distance < self.max_distance:
                            self.add_map_point(angle, distance)
            except Exception as e:
                print(f"Erreur lecture série: {e}")
            time.sleep(0.01)
    
    def reset_map(self):
        """Réinitialiser la cartographie"""
        self.map_points = []
        self.draw_map_grid()
        self.map_info.config(text="Points détectés: 0")
    
    def add_map_point(self, angle, distance):
        """Ajouter un point à la carte"""
        self.map_points.append((angle, distance))
        if len(self.map_points) > 500:  # Limiter le nombre de points
            self.map_points.pop(0)
    
    def draw_radar_grid(self):
        """Dessiner la grille du radar"""
        self.radar_canvas.delete("all")
        
        width = self.radar_canvas.winfo_width()
        height = self.radar_canvas.winfo_height()
        
        if width <= 1 or height <= 1:
            self.root.after(100, self.draw_radar_grid)
            return
        
        center_x = width // 2
        center_y = height // 2
        radius = min(center_x, center_y) - 20
        
        # Cercles concentriques
        for i in range(1, 5):
            r = radius * i / 4
            self.radar_canvas.create_oval(center_x - r, center_y - r,
                                         center_x + r, center_y + r,
                                         outline='#004400', width=1)
            # Labels de distance
            distance_label = int(self.max_distance * i / 4)
            self.radar_canvas.create_text(center_x + r - 20, center_y - 5,
                                         text=f"{distance_label}cm",
                                         fill='#006600', font=('Courier', 8))
        
        # Lignes radiales (tous les 30°)
        for angle in range(0, 360, 30):
            rad = math.radians(angle)
            x = center_x + radius * math.cos(rad - math.pi/2)
            y = center_y + radius * math.sin(rad - math.pi/2)
            self.radar_canvas.create_line(center_x, center_y, x, y,
                                         fill='#004400', width=1)
            # Labels d'angle
            label_x = center_x + (radius + 15) * math.cos(rad - math.pi/2)
            label_y = center_y + (radius + 15) * math.sin(rad - math.pi/2)
            self.radar_canvas.create_text(label_x, label_y, text=f"{angle}°",
                                         fill='#006600', font=('Courier', 9))
        
        # Point central (robot)
        self.radar_canvas.create_oval(center_x - 5, center_y - 5,
                                     center_x + 5, center_y + 5,
                                     fill='#00ff00', outline='#00ff00')
    
    def draw_map_grid(self):
        """Dessiner la grille de cartographie"""
        self.map_canvas.delete("all")
        
        width = self.map_canvas.winfo_width()
        height = self.map_canvas.winfo_height()
        
        if width <= 1 or height <= 1:
            self.root.after(100, self.draw_map_grid)
            return
        
        center_x = width // 2
        center_y = height // 2
        
        # Grille cartésienne
        grid_spacing = 50
        
        # Lignes verticales
        for x in range(0, width, grid_spacing):
            color = '#004400' if x == center_x else '#002200'
            width_line = 2 if x == center_x else 1
            self.map_canvas.create_line(x, 0, x, height, fill=color, width=width_line)
        
        # Lignes horizontales
        for y in range(0, height, grid_spacing):
            color = '#004400' if y == center_y else '#002200'
            width_line = 2 if y == center_y else 1
            self.map_canvas.create_line(0, y, width, y, fill=color, width=width_line)
        
        # Labels des axes
        self.map_canvas.create_text(center_x + 10, 15, text="Y", 
                                   fill='#00ff00', font=('Courier', 12, 'bold'))
        self.map_canvas.create_text(width - 15, center_y - 10, text="X",
                                   fill='#00ff00', font=('Courier', 12, 'bold'))
        
        # Point central (robot)
        self.map_canvas.create_oval(center_x - 6, center_y - 6,
                                   center_x + 6, center_y + 6,
                                   fill='#0000ff', outline='#0000ff')
        self.map_canvas.create_text(center_x, center_y - 15, text="ROBOT",
                                   fill='#0000ff', font=('Courier', 9, 'bold'))
    
    def animate_radar(self):
        """Animation du radar et mise à jour de la cartographie"""
        # Redessiner les grilles
        self.draw_radar_grid()
        
        width = self.radar_canvas.winfo_width()
        height = self.radar_canvas.winfo_height()
        
        if width > 1 and height > 1:
            center_x = width // 2
            center_y = height // 2
            radius = min(center_x, center_y) - 20
            
            # Dessiner les points de traînée (effet sonar)
            for i, (angle, distance, alpha) in enumerate(self.radar_points):
                if distance > 0 and distance < self.max_distance:
                    rad = math.radians(angle - 90)
                    scale = radius / self.max_distance
                    x = center_x + distance * scale * math.cos(rad)
                    y = center_y + distance * scale * math.sin(rad)
                    
                    # Intensité basée sur alpha
                    intensity = int(255 * alpha)
                    color = f"#{0:02x}{intensity:02x}{0:02x}"
                    
                    self.radar_canvas.create_oval(x - 3, y - 3, x + 3, y + 3,
                                                 fill=color, outline=color)
                
                # Décrémenter l'alpha
                self.radar_points[i] = (angle, distance, alpha * 0.92)
            
            # Retirer les points trop faibles
            self.radar_points = [(a, d, alpha) for a, d, alpha in self.radar_points if alpha > 0.05]
            
            # Ajouter le point actuel
            if self.current_distance > 0:
                self.radar_points.append((self.current_angle, self.current_distance, 1.0))
            
            # Dessiner le faisceau de balayage avec effet de rémanence
            for i in range(15):
                angle_offset = i * 2
                alpha = 1.0 - (i / 15.0)
                intensity = int(255 * alpha * 0.5)
                color = f"#{0:02x}{intensity:02x}{0:02x}"
                
                angle = self.current_angle - angle_offset
                rad = math.radians(angle - 90)
                x = center_x + radius * math.cos(rad)
                y = center_y + radius * math.sin(rad)
                
                self.radar_canvas.create_line(center_x, center_y, x, y,
                                             fill=color, width=2)
            
            # Ligne principale du radar (plus brillante)
            rad = math.radians(self.current_angle - 90)
            x = center_x + radius * math.cos(rad)
            y = center_y + radius * math.sin(rad)
            self.radar_canvas.create_line(center_x, center_y, x, y,
                                         fill='#00ff00', width=3)
            
            # Mettre à jour les infos
            self.radar_info.config(text=f"Angle: {self.current_angle}° | Distance: {self.current_distance} cm")
        
        # Mettre à jour la cartographie
        self.update_map()
        
        # Répéter l'animation
        self.root.after(50, self.animate_radar)
    
    def update_map(self):
        """Mettre à jour la vue cartographique"""
        self.draw_map_grid()
        
        width = self.map_canvas.winfo_width()
        height = self.map_canvas.winfo_height()
        
        if width > 1 and height > 1:
            center_x = width // 2
            center_y = height // 2
            scale = min(width, height) / (2 * self.max_distance) * 0.8
            
            # Dessiner tous les points de la carte
            for angle, distance in self.map_points:
                # Conversion polaire -> cartésienne
                rad = math.radians(angle - 90)  # -90 pour avoir 0° en haut
                x = center_x + distance * scale * math.cos(rad)
                y = center_y + distance * scale * math.sin(rad)
                
                # Point vert
                self.map_canvas.create_oval(x - 2, y - 2, x + 2, y + 2,
                                           fill='#00ff00', outline='#00ff00')
            
            # Point actuel en rouge (plus gros)
            if self.current_distance > 0 and self.current_distance < self.max_distance:
                rad = math.radians(self.current_angle - 90)
                x = center_x + self.current_distance * scale * math.cos(rad)
                y = center_y + self.current_distance * scale * math.sin(rad)
                
                self.map_canvas.create_oval(x - 4, y - 4, x + 4, y + 4,
                                           fill='#ff0000', outline='#ff0000')
            
            # Mettre à jour le compteur
            self.map_info.config(text=f"Points détectés: {len(self.map_points)}")

if __name__ == "__main__":
    root = tk.Tk()
    app = RadarInterface(root)
    root.mainloop()