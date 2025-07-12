import sys

def main():
    if len(sys.argv) < 2:
        print("Usage: python -m magda.main '<your prompt>'")
        sys.exit(1)
    prompt = sys.argv[1]
    # Placeholder for DAW command generation
    print(f"Received prompt: {prompt}")
    print("track(bass, serum)")
    print("track(drums, addictive_drums)")

if __name__ == "__main__":
    main() 