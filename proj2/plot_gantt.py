import pandas as pd
import matplotlib.pyplot as plt
import glob
import os

def generate_gantt_chart(csv_file, output_file):
    try:
        df = pd.read_csv(csv_file)
        if df.empty: return

        df['Duration'] = df['End'] - df['Start']

        fig, ax = plt.subplots(figsize=(12, 6)) # Slightly smaller size for reports
        
        # Color mapping
        unique_jobs = sorted(df['Job'].unique())
        cmap = plt.get_cmap('tab20')
        job_colors = {job: cmap(i % 20) for i, job in enumerate(unique_jobs)}

        for i, row in df.iterrows():
            ax.barh(y=row['Job'], width=row['Duration'], left=row['Start'], 
                    color=job_colors[row['Job']], edgecolor='black', height=0.6)

        # Clean Title (e.g., "FCFS - Run 3")
        clean_name = os.path.basename(csv_file).replace('gantt_', '').replace('.csv', '').replace('_', ' ')
        ax.set_title(f'Gantt Chart: {clean_name}')
        ax.set_xlabel('Time Quanta')
        ax.set_ylabel('Job ID')
        ax.grid(True, axis='x', linestyle='--', alpha=0.5)
        ax.set_xlim(0, max(100, df['End'].max()))

        plt.tight_layout()
        plt.savefig(output_file)
        plt.close()
        print(f"Generated: {output_file}")

    except Exception as e:
        print(f"Skipped {csv_file}: {e}")

def main():
    # FILTER: Only look for Run 3 files
    csv_files = glob.glob("*_run3.csv")
    
    if not csv_files:
        print("No CSVs found for Run 3. Make sure the C code generated them!")
        return

    print(f"Found {len(csv_files)} files for Run 3...")
    for csv_file in csv_files:
        output_file = csv_file.replace(".csv", ".png")
        generate_gantt_chart(csv_file, output_file)

if __name__ == "__main__":
    main()