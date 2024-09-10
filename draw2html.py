import sys
import plotly.graph_objects as go

def format_value(value, convert_to_storage_units):
    if convert_to_storage_units:
        if value >= 1024:
            return f"{value / 1024:.2f} GB"
        else:
            return f"{value:.2f} MB"
    else:
        return f"{value:.2f}"

def plot_from_file(filename):
    with open(filename, 'r') as file:
        lines = file.readlines()

    plot_title, xlabel, ylabel = lines[0].strip().split(',')
    convert_to_storage_units = "Time" not in ylabel
    line_titles = lines[1].strip().split(',')
    x_labels = lines[2].strip().split(',')

    data_points = []
    for line in lines[3:]:
        if line.strip():
            try:
                data_points.append(list(map(float, line.strip().split(','))))
            except ValueError as e:
                print(f"Warning: Skipping line due to conversion error: {e}")
                continue

    if not data_points or len(data_points[0]) != len(x_labels):
        print("Error: Data points count does not match the number of x labels.")
        return

    all_y_data = [value for sublist in data_points for value in sublist]

    max_value = max(all_y_data)
    y_ticks = [tick for tick in range(0, int(max_value) + 1, int(max_value // 10) or 1)]

    formatted_y_ticks = [format_value(tick, convert_to_storage_units) for tick in y_ticks]

    fig = go.Figure()

    for i, title in enumerate(line_titles):
        if i < len(data_points):
            y_data = data_points[i]
            hover_texts = [f"{x_labels[j]} : {format_value(y, convert_to_storage_units)}" 
               for j, y in enumerate(y_data)]
            fig.add_trace(go.Scatter(x=x_labels, y=y_data, mode='lines+markers', name=title, hovertext=hover_texts, hoverinfo='text'))

    fig.update_layout(
        title=plot_title,
        xaxis_title=xlabel,
        yaxis_title=ylabel,
        xaxis=dict(tickangle=45),
        yaxis=dict(
            tickmode='array',
            tickvals=y_ticks,
            ticktext=formatted_y_ticks
        )
    )

    output_filename = filename.rsplit('.', 1)[0] + '.html'
    fig.write_html(output_filename)
    print(f"Plot saved to {output_filename}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python draw2html.py <input_file_path>")
        sys.exit(1)
    input_file_path = sys.argv[1]
    plot_from_file(input_file_path)
