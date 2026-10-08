import type { ButtonHTMLAttributes, ReactNode } from 'react';

/** The one container everything sits in: bordered tile, centered bold title. */
export function Tile({ title, children, className = '', actions }: { title?: string; children: ReactNode; className?: string; actions?: ReactNode }) {
  return (
    <section className={`tile ${className}`}>
      {title && <h2 className="tile-title">{title}</h2>}
      {actions && <div className="tile-actions">{actions}</div>}
      <div className="tile-body">{children}</div>
    </section>
  );
}

export function Button({ variant = 'secondary', className = '', ...props }: ButtonHTMLAttributes<HTMLButtonElement> & { variant?: 'primary' | 'secondary' | 'danger' }) {
  return <button className={`btn btn-${variant} ${className}`} {...props} />;
}

export function Badge({ children, tone = 'neutral' }: { children: ReactNode; tone?: 'neutral' | 'good' | 'bad' | 'warn' | 'info' }) {
  return <span className={`badge badge-${tone}`}>{children}</span>;
}

/** Row of mutually exclusive choices (replaces sliders/selects with plain buttons). */
export function Segmented<T extends string | number>({ options, value, onChange, label }: {
  options: { value: T; label: string }[]; value: T; onChange: (v: T) => void; label: string;
}) {
  return (
    <div className="segmented" role="group" aria-label={label}>
      {options.map(o => (
        <button key={String(o.value)} type="button" aria-pressed={o.value === value} className={o.value === value ? 'on' : ''} onClick={() => onChange(o.value)}>
          {o.label}
        </button>
      ))}
    </div>
  );
}

/** Toggle chip for multi-select (kernels, targets). */
export function Chip({ label, on, onClick, hint, disabled }: { label: string; on: boolean; onClick: () => void; hint?: string; disabled?: boolean }) {
  return (
    <button type="button" aria-pressed={on} disabled={disabled} title={hint} className={`chip ${on ? 'on' : ''}`} onClick={onClick}>
      {label}
    </button>
  );
}

export function Check({ label, checked, onChange, hint }: { label: string; checked: boolean; onChange: (v: boolean) => void; hint?: string }) {
  return (
    <label className="check">
      <input type="checkbox" checked={checked} onChange={e => onChange(e.target.checked)} />
      <span>{label}{hint && <small>{hint}</small>}</span>
    </label>
  );
}

export function Empty({ children }: { children: ReactNode }) {
  return <div className="empty">{children}</div>;
}

/** Compact table in the same grid style as the tiles. */
export function DataTable({ head, children, className = '', widths }: { head: ReactNode[]; children: ReactNode; className?: string; widths?: string[] }) {
  return (
    <div className="table-wrap">
      <table className={`data ${className}`}>
        {widths && <colgroup>{widths.map((w, i) => <col key={i} style={{ width: w }} />)}</colgroup>}
        <thead><tr>{head.map((h, i) => <th key={i}>{h}</th>)}</tr></thead>
        <tbody>{children}</tbody>
      </table>
    </div>
  );
}
