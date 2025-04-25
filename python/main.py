import sys
import glob
import serial
import pyautogui
pyautogui.PAUSE=0.0
import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
from time import sleep, time
from PIL import Image, ImageTk

from pynput import keyboard, mouse

teclado = keyboard.Controller()
click = mouse.Controller()
i_inv = 1
inv = False
mis = False
lista_macro = []
tempo_medido = 0
estado_macro = 0
qnt_comandos = 0

def ler_macro():
    global lista_macro, estado_macro

    if not lista_macro or len(lista_macro) < 2:
        estado_macro = 0
        return

    t_base = lista_macro[0][1]

    for i in range(len(lista_macro)):
        axis, t, value = lista_macro[i]

        # Tempo até o próximo comando
        if i == 0:
            t_atual = t - t_base 
        else: 
            t_atual = lista_macro[i][1] - lista_macro[i - 1][1]

        sleep(max(0, t_atual))

        move(axis, value)

    estado_macro = 0  # Finaliza leitura

def move(axis, value):
    global i_inv, inv, mis, lista_macro, tempo_medido, estado_macro, qnt_comandos
    inv_teclas = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0','-','=']
    """Move o mouse de acordo com o eixo e valor recebidos."""
    # print(f"AXIS: {axis} \n")

    print(f"Estado: {estado_macro}, Tamanho: {qnt_comandos} \n")

    if (estado_macro == 1):    # Criando o macro
        if (qnt_comandos == 0):
            lista_macro = []
        if (axis != 8):
            lista_macro.append([axis, time(), value])
            qnt_comandos += 1

    if (axis == 0):
        if (inv):
            pyautogui.moveRel(0, -value)
        elif (mis):
            pyautogui.moveRel(0, -value)
        else:
            if (value > 0):
                teclado.release('s')
                teclado.press('w')
            elif (-30 <= value <= 30):
                teclado.release('s')
                teclado.release('w')    
            else:
                teclado.release('w')
                teclado.press('s')

    elif (axis == 1):
        if (inv):
            pyautogui.moveRel(value, 0)
        elif(mis):
            pyautogui.moveRel(value, 0)
        else:
            if (value < 0):
                teclado.release('d')
                teclado.press('a')
            elif (-30 <= value <= 30):
                teclado.release('a')
                teclado.release('d')
            else:
                teclado.release('a')
                teclado.press('d')

    elif (axis == 2):
        if (value == 1):
            click.press(mouse.Button.left)
        else:
            click.release(mouse.Button.left)
    
    elif (axis == 3):
        if (value == 1):
            teclado.press('x')
        else:
            teclado.release('x')
    
    elif (axis == 4):
        if (value == 1):
            teclado.press('f')
            mis = not mis
        else:
            teclado.release('f')

    elif (axis == 5):
        if (value == 1):
            teclado.press('e')
            inv = not inv
        else:
            teclado.release('e')
    
    elif (axis == 6):
        if (value == 1):
            i_inv = (i_inv-1) % 12
            teclado.press(inv_teclas[i_inv])
        else:
            teclado.release(inv_teclas[i_inv])
    
    elif (axis == 7):
        if (value == 1):
            i_inv = (i_inv+1) % 12
            teclado.press(inv_teclas[i_inv])
        else:
            teclado.release(inv_teclas[i_inv])
    
    elif (axis == 8):
        if (value == 1):
            tempo_medido = time()
        else:
            if (estado_macro == 1):                 # Estava guardando info no macro
                estado_macro = 0
            elif (time() - tempo_medido > 1):       # Segurou
                estado_macro = -1  # Lendo o macro
                ler_macro()
            else:                                   # Não segurou
                qnt_comandos = 0
                estado_macro = 1   # Criando o macro


def controle(ser):
    """
    Loop principal que lê bytes da porta serial em loop infinito.
    Aguarda o byte 0xFF e então lê 3 bytes: axis (1 byte) + valor (2 bytes).
    """
    global inv
    while True:
        # Aguardar byte de sincronização
        sync_byte = ser.read(size=1)
        if not sync_byte:
            continue
        if sync_byte[0] == 0xFF:
            # Ler 3 bytes (axis + valor(2b))
            data = ser.read(size=3)
            if len(data) < 3:
                continue
            # print(f"{data} e {inv}\n")
            axis, value = parse_data(data)

            move(axis, value)

def serial_ports():
    """Retorna uma lista das portas seriais disponíveis na máquina."""
    ports = []
    if sys.platform.startswith('win'):
        # Windows
        for i in range(1, 256):
            port = f'COM{i}'
            try:
                s = serial.Serial(port)
                s.close()
                ports.append(port)
            except (OSError, serial.SerialException):
                pass
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # Linux/Cygwin
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        # macOS
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Plataforma não suportada para detecção de portas seriais.')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result

def parse_data(data):
    """Interpreta os dados recebidos do buffer (axis + valor)."""
    axis = data[0]
    value = int.from_bytes(data[1:3], byteorder='little', signed=True)
    return axis, value

def conectar_porta(port_name, root, botao_conectar, status_label, mudar_cor_circulo):
    """Abre a conexão com a porta selecionada e inicia o loop de leitura."""
    if not port_name:
        messagebox.showwarning("Aviso", "Selecione uma porta serial antes de conectar.")
        return

    try:
        ser = serial.Serial(port_name, 115200, timeout=1)
        status_label.config(text=f"Conectado em {port_name}", foreground="green")
        mudar_cor_circulo("green")
        botao_conectar.config(text="Conectado")  # Update button text to indicate connection
        print(f"Conectado em {port_name}")
        ser.write(b'c') 
        root.update()

        # Inicia o loop de leitura (bloqueante).
        controle(ser)

    except KeyboardInterrupt:
        print("Encerrando via KeyboardInterrupt.")

    except Exception as e:
        messagebox.showerror("Erro de Conexão", f"Não foi possível conectar em {port_name}.\nErro: {e}")
        mudar_cor_circulo("red")

    finally:
        if ser and ser.is_open:
            try:
                ser.write(b'e')
                sleep(0.1)  
                ser.close()
                print("Porta serial fechada.")
            except Exception as e:
                print(f"Erro ao fechar a porta: {e}")
        status_label.config(text="Conexão encerrada.", foreground="red")
        mudar_cor_circulo("red")

def criar_janela():
    root = tk.Tk()
    root.title("Stardew Valley Controller")
    root.geometry("736x414")  # Matches the image size
    root.resizable(False, False)

    # Stardew Valley-inspired color palette
    bg_color = "#f5e6c8"  # Soft cream for widget backgrounds
    text_color = "#000000"  # Black for text
    accent_color = "#8b5e3c"  # Wooden brown for buttons
    active_color = "#a67c00"  # Golden yellow for active states

    # Create a Canvas to hold the background image and other widgets
    canvas = tk.Canvas(root, width=736, height=414, highlightthickness=0)
    canvas.pack(fill="both", expand=True)

    # Load and set background image using Pillow
    try:
        # Load the image with Pillow
        image = Image.open("./img/stardew_img.jpg")
        # Convert to PhotoImage
        bg_image = ImageTk.PhotoImage(image)
        # Draw the image on the canvas
        canvas.create_image(0, 0, image=bg_image, anchor="nw")
        canvas.image = bg_image  # Prevent garbage collection
    except Exception as e:
        print(f"Error loading image: {e}")
        canvas.configure(bg=bg_color)  # Fallback if image fails

    # Configure ttk style with Stardew Valley aesthetic
    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure("TLabel", background=bg_color, foreground=text_color, font=("Pixelify Sans", 12))
    style.configure("TButton", font=("Pixelify Sans", 10, "bold"),
                    foreground=text_color, background=accent_color, borderwidth=2, relief="raised")
    style.map("TButton", background=[("active", active_color)], relief=[("active", "sunken")])
    style.configure("Accent.TButton", font=("Pixelify Sans", 12, "bold"),
                    foreground=text_color, background=accent_color, padding=8)
    style.map("Accent.TButton", background=[("active", active_color)])

    # Combobox styling to match Stardew Valley's wooden UI
    style.configure("TCombobox",
                    fieldbackground=bg_color,
                    background=accent_color,
                    foreground=text_color,
                    padding=4,
                    font=("Pixelify Sans", 10))
    style.map("TCombobox", fieldbackground=[("readonly", bg_color)])

    # Title with Stardew Valley-inspired font
    titulo_label = ttk.Label(root, text="Stardew Valley", font=("Pixelify Sans", 20, "bold"))
    canvas.create_window(736//2, 50, window=titulo_label)  # Place title at the top center

    porta_var = tk.StringVar(value="")

    # Connect button with wooden button style
    botao_conectar = ttk.Button(
        root,
        text="Conectar ao controle",
        style="Accent.TButton",
        command=lambda: conectar_porta(porta_var.get(), root, botao_conectar, status_label, mudar_cor_circulo)
    )
    canvas.create_window(736//2, 120, window=botao_conectar)  # Place button below the title

    # Status label with a background for readability
    status_label = tk.Label(root, text="Aguardando seleção de porta", font=("Pixelify Sans", 8),
                            bg=bg_color, fg=text_color)
    canvas.create_window(50, 414 - 20, window=status_label, anchor="w")  # Place at bottom left

    # Port selection dropdown
    portas_disponiveis = serial_ports()
    if portas_disponiveis:
        porta_var.set(portas_disponiveis[0])
    port_dropdown = ttk.Combobox(root, textvariable=porta_var,
                                 values=portas_disponiveis, state="readonly", width=12)
    canvas.create_window(736//2 + 148, 414 - 20, window=port_dropdown)  # Place at bottom center

    # Status circle (canvas) with a background for visibility
    circle_canvas = tk.Canvas(root, width=20, height=20, highlightthickness=0, bg=bg_color)
    circle_item = circle_canvas.create_oval(2, 2, 18, 18, fill="#a62e2e", outline="")  # Reddish for disconnected
    canvas.create_window(736 - 50, 414 - 20, window=circle_canvas, anchor="e")  # Place at bottom right

    def mudar_cor_circulo(cor):
        circle_canvas.itemconfig(circle_item, fill=cor)

    root.mainloop()


if __name__ == "__main__":
    criar_janela()
