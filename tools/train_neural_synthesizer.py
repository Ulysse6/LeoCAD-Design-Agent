#!/usr/bin/env python3
"""
Trains a lightweight Neural Vision-to-Brick Structural Model on dataset/training_pairs.jsonl.
Learns to predict 3D bounding geometry, layer density, step counts, and brick proportions
directly from multimodal image features, text semantics, and piece budgets.

Output: models/vlm_neural_weights.json (embedded directly into LeoCAD's Hybrid Synthesizer).
"""

import json
import os
import math
import random
from PIL import Image

DATASET_FILE = "dataset/training_pairs.jsonl"
WEIGHTS_FILE = "models/vlm_neural_weights.json"

# Feature vector dimensions: 32 inputs
# Targets: 12 outputs:
#   [0] Width (studs / 30)
#   [1] Height (layers / 25)
#   [2] Depth (studs / 30)
#   [3] Step count (/ 30)
#   [4] Ratio 2x4 bricks
#   [5] Ratio 2x2 bricks
#   [6] Ratio 1x2 bricks
#   [7] Ratio 1x1 bricks
#   [8] Ratio slopes
#   [9] Ratio plates
#   [10] Dominant color code (/ 100)
#   [11] Complexity factor (pieces / volume)

def extract_input_features(img_path, prompt_text, target_pcs):
    feat = [0.0] * 32

    # 1. Image Spatial & Density Features (16 dims)
    if img_path and os.path.exists(img_path):
        try:
            img = Image.open(img_path).convert("L").resize((16, 16))
            pixels = list(img.getdata())
            # 8 vertical slices (density from bottom to top)
            for y in range(8):
                slice_vals = [pixels[(y * 2 + dy) * 16 + x] for dy in range(2) for x in range(16)]
                feat[y] = sum(1 for v in slice_vals if v < 240) / len(slice_vals)

            # 8 horizontal slices (density from left to right)
            for x in range(8):
                col_vals = [pixels[y * 16 + (x * 2 + dx)] for dx in range(2) for y in range(16)]
                feat[8 + x] = sum(1 for v in col_vals if v < 240) / len(col_vals)
        except Exception:
            pass

    # 2. Aspect Ratio (1 dim)
    if img_path and os.path.exists(img_path):
        try:
            with Image.open(img_path) as im:
                feat[16] = min(3.0, max(0.2, im.width / max(1, im.height))) / 3.0
        except Exception:
            feat[16] = 0.33
    else:
        feat[16] = 0.33

    # 3. Piece Budget (1 dim)
    feat[17] = math.log10(max(10, min(1000, target_pcs))) / 3.0

    # 4. Semantic Domain Keywords (8 dims)
    p = prompt_text.lower()
    feat[18] = 1.0 if any(k in p for k in ["car", "truck", "racer", "pickup", "van", "wagon", "buggy"]) else 0.0
    feat[19] = 1.0 if any(k in p for k in ["train", "locomotive", "rail", "chief", "engine"]) else 0.0
    feat[20] = 1.0 if any(k in p for k in ["plane", "airplane", "jet", "aircraft", "helicopter"]) else 0.0
    feat[21] = 1.0 if any(k in p for k in ["space", "spaceship", "rocket", "shuttle", "x-wing"]) else 0.0
    feat[22] = 1.0 if any(k in p for k in ["house", "building", "villa", "lodge", "cottage"]) else 0.0
    feat[23] = 1.0 if any(k in p for k in ["tower", "castle", "landmark", "eiffel", "white house", "pisa", "big ben"]) else 0.0
    feat[24] = 1.0 if any(k in p for k in ["boat", "ship", "speedboat", "yacht"]) else 0.0
    feat[25] = 1.0 if any(k in p for k in ["robot", "mech", "droid"]) else 0.0

    # 5. Color Keywords (6 dims)
    feat[26] = 1.0 if "red" in p else 0.0
    feat[27] = 1.0 if "blue" in p else 0.0
    feat[28] = 1.0 if "yellow" in p else 0.0
    feat[29] = 1.0 if "black" in p else 0.0
    feat[30] = 1.0 if "white" in p else 0.0
    feat[31] = 1.0 if any(k in p for k in ["gray", "grey"]) else 0.0

    return feat


def parse_ground_truth(response_text):
    targets = [0.0] * 12
    pieces = []
    current_step = 1

    part_counts = {}
    color_counts = {}

    for line in response_text.splitlines():
        line = line.strip()
        if line.startswith("0 STEP"):
            current_step += 1
            continue
        if line.startswith("1 "):
            tokens = line.split()
            if len(tokens) >= 15:
                try:
                    c = int(tokens[1])
                    x, y, z = float(tokens[2]), float(tokens[3]), float(tokens[4])
                    part = tokens[14].lower().replace(".dat", "")
                    pieces.append((c, x, y, z, part))
                    part_counts[part] = part_counts.get(part, 0) + 1
                    color_counts[c] = color_counts.get(c, 0) + 1
                except ValueError:
                    pass

    if not pieces:
        return None

    xs = [p[1] for p in pieces]
    ys = [p[2] for p in pieces]
    zs = [p[3] for p in pieces]

    width_studs = (max(xs) - min(xs)) / 20.0
    height_layers = (max(ys) - min(ys)) / 24.0
    depth_studs = (max(zs) - min(zs)) / 20.0

    total_pcs = len(pieces)
    targets[0] = min(1.0, width_studs / 30.0)
    targets[1] = min(1.0, height_layers / 25.0)
    targets[2] = min(1.0, depth_studs / 30.0)
    targets[3] = min(1.0, current_step / 30.0)

    # Brick type proportions
    c_2x4 = part_counts.get("3001", 0) + part_counts.get("3002", 0)
    c_2x2 = part_counts.get("3003", 0)
    c_1x2 = part_counts.get("3004", 0) + part_counts.get("3010", 0)
    c_1x1 = part_counts.get("3005", 0)
    c_slopes = sum(v for k, v in part_counts.items() if "3039" in k or "3040" in k or "slope" in k)
    c_plates = sum(v for k, v in part_counts.items() if "3020" in k or "3022" in k or "3023" in k or "plate" in k)

    targets[4] = c_2x4 / total_pcs
    targets[5] = c_2x2 / total_pcs
    targets[6] = c_1x2 / total_pcs
    targets[7] = c_1x1 / total_pcs
    targets[8] = c_slopes / total_pcs
    targets[9] = c_plates / total_pcs

    dom_color = max(color_counts, key=color_counts.get) if color_counts else 16
    targets[10] = dom_color / 100.0
    vol = max(1.0, width_studs * height_layers * depth_studs)
    targets[11] = min(1.0, total_pcs / vol)

    return targets


# ============================================================================
# Pure Python Multi-Layer Perceptron (Backprop + Adam)
# ============================================================================

class NeuralVLMModel:
    def __init__(self, d_in=32, d_h1=48, d_h2=24, d_out=12):
        self.d_in = d_in
        self.d_h1 = d_h1
        self.d_h2 = d_h2
        self.d_out = d_out

        # He initialization
        def init_weights(din, dout):
            scale = math.sqrt(2.0 / din)
            return [[random.gauss(0, scale) for _ in range(dout)] for _ in range(din)]

        self.W1 = init_weights(d_in, d_h1)
        self.b1 = [0.0] * d_h1
        self.W2 = init_weights(d_h1, d_h2)
        self.b2 = [0.0] * d_h2
        self.W3 = init_weights(d_h2, d_out)
        self.b3 = [0.0] * d_out

        # Adam optimizer state
        self.mW1 = [[0.0] * d_h1 for _ in range(d_in)]
        self.vW1 = [[0.0] * d_h1 for _ in range(d_in)]
        self.mb1 = [0.0] * d_h1
        self.vb1 = [0.0] * d_h1

        self.mW2 = [[0.0] * d_h2 for _ in range(d_h1)]
        self.vW2 = [[0.0] * d_h2 for _ in range(d_h1)]
        self.mb2 = [0.0] * d_h2
        self.vb2 = [0.0] * d_h2

        self.mW3 = [[0.0] * d_out for _ in range(d_h2)]
        self.vW3 = [[0.0] * d_out for _ in range(d_h2)]
        self.mb3 = [0.0] * d_out
        self.vb3 = [0.0] * d_out

        self.t = 0

    def forward(self, x):
        # Layer 1
        h1 = [0.0] * self.d_h1
        for j in range(self.d_h1):
            val = self.b1[j]
            for i in range(self.d_in):
                val += x[i] * self.W1[i][j]
            h1[j] = max(0.0, val)  # ReLU

        # Layer 2
        h2 = [0.0] * self.d_h2
        for j in range(self.d_h2):
            val = self.b2[j]
            for i in range(self.d_h1):
                val += h1[i] * self.W2[i][j]
            h2[j] = max(0.0, val)  # ReLU

        # Output Layer (Linear / Sigmoid clamped)
        out = [0.0] * self.d_out
        for j in range(self.d_out):
            val = self.b3[j]
            for i in range(self.d_h2):
                val += h2[i] * self.W3[i][j]
            # Sigmoid activation to keep outputs in [0, 1]
            out[j] = 1.0 / (1.0 + math.exp(-max(-15.0, min(15.0, val))))

        return h1, h2, out

    def train_step(self, x, y_true, lr=0.003):
        self.t += 1
        beta1, beta2, eps = 0.9, 0.999, 1e-8

        h1, h2, y_pred = self.forward(x)

        # Loss and gradient at output
        loss = 0.0
        d_out = [0.0] * self.d_out
        for j in range(self.d_out):
            diff = y_pred[j] - y_true[j]
            loss += diff * diff
            # Sigmoid derivative
            d_out[j] = 2.0 * diff * y_pred[j] * (1.0 - y_pred[j])
        loss /= self.d_out

        # Gradients for W3, b3
        d_h2 = [0.0] * self.d_h2
        for j in range(self.d_out):
            grad_b = d_out[j]
            self.mb3[j] = beta1 * self.mb3[j] + (1 - beta1) * grad_b
            self.vb3[j] = beta2 * self.vb3[j] + (1 - beta2) * (grad_b * grad_b)
            m_hat = self.mb3[j] / (1 - beta1 ** self.t)
            v_hat = self.vb3[j] / (1 - beta2 ** self.t)
            self.b3[j] -= lr * m_hat / (math.sqrt(v_hat) + eps)

            for i in range(self.d_h2):
                grad_w = h2[i] * d_out[j]
                d_h2[i] += self.W3[i][j] * d_out[j]
                self.mW3[i][j] = beta1 * self.mW3[i][j] + (1 - beta1) * grad_w
                self.vW3[i][j] = beta2 * self.vW3[i][j] + (1 - beta2) * (grad_w * grad_w)
                m_hat = self.mW3[i][j] / (1 - beta1 ** self.t)
                v_hat = self.vW3[i][j] / (1 - beta2 ** self.t)
                self.W3[i][j] -= lr * m_hat / (math.sqrt(v_hat) + eps)

        # Gradients for W2, b2 (ReLU)
        d_h1 = [0.0] * self.d_h1
        for j in range(self.d_h2):
            if h2[j] <= 0.0:
                continue
            grad_b = d_h2[j]
            self.mb2[j] = beta1 * self.mb2[j] + (1 - beta1) * grad_b
            self.vb2[j] = beta2 * self.vb2[j] + (1 - beta2) * (grad_b * grad_b)
            m_hat = self.mb2[j] / (1 - beta1 ** self.t)
            v_hat = self.vb2[j] / (1 - beta2 ** self.t)
            self.b2[j] -= lr * m_hat / (math.sqrt(v_hat) + eps)

            for i in range(self.d_h1):
                grad_w = h1[i] * d_h2[j]
                d_h1[i] += self.W2[i][j] * d_h2[j]
                self.mW2[i][j] = beta1 * self.mW2[i][j] + (1 - beta1) * grad_w
                self.vW2[i][j] = beta2 * self.vW2[i][j] + (1 - beta2) * (grad_w * grad_w)
                m_hat = self.mW2[i][j] / (1 - beta1 ** self.t)
                v_hat = self.vW2[i][j] / (1 - beta2 ** self.t)
                self.W2[i][j] -= lr * m_hat / (math.sqrt(v_hat) + eps)

        # Gradients for W1, b1 (ReLU)
        for j in range(self.d_h1):
            if h1[j] <= 0.0:
                continue
            grad_b = d_h1[j]
            self.mb1[j] = beta1 * self.mb1[j] + (1 - beta1) * grad_b
            self.vb1[j] = beta2 * self.vb1[j] + (1 - beta2) * (grad_b * grad_b)
            m_hat = self.mb1[j] / (1 - beta1 ** self.t)
            v_hat = self.vb1[j] / (1 - beta2 ** self.t)
            self.b1[j] -= lr * m_hat / (math.sqrt(v_hat) + eps)

            for i in range(self.d_in):
                grad_w = x[i] * d_h1[j]
                self.mW1[i][j] = beta1 * self.mW1[i][j] + (1 - beta1) * grad_w
                self.vW1[i][j] = beta2 * self.vW1[i][j] + (1 - beta2) * (grad_w * grad_w)
                m_hat = self.mW1[i][j] / (1 - beta1 ** self.t)
                v_hat = self.vW1[i][j] / (1 - beta2 ** self.t)
                self.W1[i][j] -= lr * m_hat / (math.sqrt(v_hat) + eps)

        return loss

    def export_weights(self):
        return {
            "d_in": self.d_in,
            "d_h1": self.d_h1,
            "d_h2": self.d_h2,
            "d_out": self.d_out,
            "W1": [[round(v, 5) for v in row] for row in self.W1],
            "b1": [round(v, 5) for v in self.b1],
            "W2": [[round(v, 5) for v in row] for row in self.W2],
            "b2": [round(v, 5) for v in self.b2],
            "W3": [[round(v, 5) for v in row] for row in self.W3],
            "b3": [round(v, 5) for v in self.b3],
        }


def main():
    print("=" * 65)
    print("  TRAINING LEOCAD NEURAL VISION-TO-BRICK STRUCTURAL MODEL")
    print("=" * 65)

    if not os.path.exists(DATASET_FILE):
        print(f"Error: {DATASET_FILE} not found.")
        return

    print(f"[*] Loading and extracting features from {DATASET_FILE}...")
    dataset = []
    with open(DATASET_FILE, "r", encoding="utf-8") as f:
        for line in f:
            item = json.loads(line)
            img_path = item.get("image", "")
            prompt = item["conversations"][0]["value"]
            resp = item["conversations"][1]["value"]

            # Extract piece count from prompt
            pcs = 150
            import re
            m = re.search(r"(\d+)\s*(?:pieces|piece)", prompt)
            if m:
                pcs = int(m.group(1))

            x = extract_input_features(img_path, prompt, pcs)
            y = parse_ground_truth(resp)
            if y is not None:
                dataset.append((x, y))

    print(f"[*] Prepared {len(dataset)} valid training samples.")

    # Train / Val Split (85% / 15%)
    random.seed(42)
    random.shuffle(dataset)
    split_idx = int(len(dataset) * 0.85)
    train_data = dataset[:split_idx]
    val_data = dataset[split_idx:]

    print(f"[*] Train set: {len(train_data)} samples | Val set: {len(val_data)} samples.")
    print("[*] Initializing 3-layer neural network (32 -> 48 -> 24 -> 12)...")

    model = NeuralVLMModel(d_in=32, d_h1=48, d_h2=24, d_out=12)

    epochs = 20
    print(f"[*] Beginning training across {epochs} epochs...")
    print("-" * 65)
    print(f"{'Epoch':<8} | {'Train MSE Loss':<18} | {'Val MSE Loss':<18} | {'Convergence'}")
    print("-" * 65)

    for ep in range(1, epochs + 1):
        random.shuffle(train_data)
        train_loss = 0.0
        for x, y in train_data:
            l = model.train_step(x, y, lr=0.004)
            train_loss += l
        train_loss /= len(train_data)

        # Validation evaluation
        val_loss = 0.0
        for x, y in val_data:
            _, _, pred = model.forward(x)
            l = sum((pred[j] - y[j]) ** 2 for j in range(len(y))) / len(y)
            val_loss += l
        val_loss /= len(val_data)

        progress_bar = "█" * int(min(20, max(1, 20 * (1.0 - val_loss * 15))))
        print(f"Epoch {ep:<2}/20 | Loss: {train_loss:.5f}       | Val: {val_loss:.5f}         | {progress_bar}")

    print("-" * 65)
    print(f"[✓] Training complete! Final Validation MSE Loss: {val_loss:.5f}")

    os.makedirs("models", exist_ok=True)
    weights = model.export_weights()
    with open(WEIGHTS_FILE, "w", encoding="utf-8") as f:
        json.dump(weights, f, indent=2)

    print(f"[✓] Saved fine-tuned neural weights to: {WEIGHTS_FILE} ({os.path.getsize(WEIGHTS_FILE)} bytes)")

    # Test sample prediction
    sample_x, sample_y = val_data[0]
    _, _, sample_pred = model.forward(sample_x)
    print("\n--- Validation Sample Prediction vs Ground Truth ---")
    labels = ["Width (studs)", "Height (layers)", "Depth (studs)", "Step Count",
              "2x4 Ratio", "2x2 Ratio", "1x2 Ratio", "1x1 Ratio", "Slope Ratio", "Plate Ratio", "Color Code", "Complexity"]
    for i, label in enumerate(labels):
        print(f"  {label:<18}: Predicted = {sample_pred[i]:.3f} | Ground Truth = {sample_y[i]:.3f}")


if __name__ == "__main__":
    main()
