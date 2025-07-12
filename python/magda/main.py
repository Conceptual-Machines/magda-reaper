import argparse

from .pipeline import MAGDAPipeline


def main():
    parser = argparse.ArgumentParser(
        description="MAGDA: Multi Agent Generative DAW API. Translate natural language prompts into DAW commands."
    )
    parser.add_argument(
        "prompt",
        type=str,
        help="The natural language prompt to translate into DAW commands.",
    )
    args = parser.parse_args()

    # Initialize the MAGDA pipeline
    pipeline = MAGDAPipeline()

    # Process the prompt
    result = pipeline.process_prompt(args.prompt)

    # Display final results
    print("\n" + "=" * 50)
    print("FINAL DAW COMMANDS:")
    print("=" * 50)
    for i, command in enumerate(result["daw_commands"], 1):
        print(f"{i}. {command}")


if __name__ == "__main__":
    main()
